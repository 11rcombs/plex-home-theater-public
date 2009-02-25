/*
 *  CoreAudioAUHAL.cpp
 *  Plex
 *
 *  Created by Ryan Walklin on 9/6/08.
 *  Copyright 2008 Ryan Walklin. All rights reserved.
 *  Based on PortAudioDirectSound.cpp/ALSADirectSound.cpp from XBMC
 *  CoreAudio HAL interface code from VLC (www.videolan.org)
 *
 */

#ifdef __APPLE__
/*
 * XBoxMediaPlayer
 * Copyright (c) 2002 d7o3g4q and RUNTiME
 * Portions Copyright (c) by the authors of ffmpeg and xvid
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Founda	tion; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnitProperties.h>
#include <AudioUnit/AudioUnitParameters.h>
#include <AudioUnit/AudioOutputUnit.h>
#include <AudioToolbox/AudioFormat.h>

#include "stdafx.h"
#include "CoreAudioAUHAL.h"
#include "Settings.h"
#include "AudioDecoder.h"

bool CoreAudioAUHAL::s_lastPlayWasSpdif = false;

/**
 * High precision date or time interval
 *
 * Store a high precision date or time interval. The maximum precision is the
 * microsecond, and a 64 bits integer is used to avoid overflows (maximum
 * time interval is then 292271 years, which should be long enough for any
 * video). Dates are stored as microseconds since a common date (usually the
 * epoch). Note that date and time intervals can be manipulated using regular
 * arithmetic operators, and that no special functions are required.
 */
#define BUFSIZE 0xffffff

struct CoreAudioDeviceParameters
{
  /* AUHAL specific */
	AudioDeviceID				device_id;
	Component                   au_component;   /* The Audiocomponent we use */
	AudioUnit                   au_unit;        /* The AudioUnit we use */
	PaUtilRingBuffer*			outputBuffer;
	void*						outputBufferData;
	int						hardwareFrameLatency;
	bool						b_digital;      /* Are we running in digital mode? */

	/* CoreAudio SPDIF mode specific */
  AudioDeviceIOProcID			sInputIOProcID;
  pid_t                       i_hog_pid;      /* The keep the pid of our hog status */
  AudioStreamID               i_stream_id;    /* The StreamID that has a cac3 streamformat */
  int                         i_stream_index; /* The index of i_stream_id in an AudioBufferList */
  AudioStreamBasicDescription stream_format;  /* The format we changed the stream to */
  AudioStreamBasicDescription sfmt_revert;    /* The original format of the stream */
  bool                  b_revert;       /* Wether we need to revert the stream format */
  bool                  b_changed_mixing;/* Wether we need to set the mixing mode back */
};

/*****************************************************************************
 * Open: open macosx audio output
 *****************************************************************************/
CoreAudioAUHAL::CoreAudioAUHAL(const CStdString& strName, const char *strCodec, int channels, unsigned int sampleRate, int bitsPerSample, bool passthrough, bool isMusic, int packetSize)
  : m_bIsInitialized(false)
{
  OSStatus                err = noErr;
  UInt32                  i_param_size = 0;
	
	if (g_audioConfig.UseDigitalOutput() &&
		channels > 2 &&
		!passthrough)
	{
		// Enable AC3 passthrough for digital devices
		int mpeg_remapping = 0;
		if (strCodec == "AAC" || strCodec == "DTS") mpeg_remapping = 1; // DTS uses MPEG channel mapping
<<<<<<< HEAD:xbmc/cores/CoreAudioAUHAL.cpp
		if (ac3encoder_init(&m_ac3encoder, channels, sampleRate, bitsPerSample, mpeg_remapping) == -1)
		{
			m_bIsInitialized = false;
			return;
		}
		else
		{
			m_bEncodeAC3 = true;
			ac3_framebuffer = (unsigned char *)calloc(packetSize, 1);
		}
		
=======
		ac3encoder_init(&m_ac3encoder, channels, sampleRate, bitsPerSample, mpeg_remapping);
		m_bEncodeAC3 = true;
		ac3_framebuffer = (unsigned char *)calloc(packetSize, 1);
>>>>>>> Pull AC3 encoder into Core Audio HAL:xbmc/cores/CoreAudioAUHAL.cpp
	}
	else
	{
		m_bEncodeAC3 = false;
		ac3_framebuffer = NULL;
	}

	CLog::Log(LOGNOTICE, "Asked to create device:   [%s]", strName.c_str());
	CLog::Log(LOGNOTICE, "Device should be digital: [%d]\n", passthrough);
	CLog::Log(LOGNOTICE, "CoreAudio S/PDIF mode:    [%d]\n", g_audioConfig.UseDigitalOutput());
	CLog::Log(LOGNOTICE, "Music mode:               [%d]\n", isMusic);
	CLog::Log(LOGNOTICE, "Channels:                 [%d]\n", channels);
	CLog::Log(LOGNOTICE, "Sample Rate:              [%d]\n", sampleRate);
	CLog::Log(LOGNOTICE, "BitsPerSample:            [%d]\n", bitsPerSample);
	CLog::Log(LOGNOTICE, "PacketSize:               [%d]\n", packetSize);

	/* Allocate structure */
	deviceParameters = (CoreAudioDeviceParameters*)calloc(sizeof(CoreAudioDeviceParameters), 1);
	if (!deviceParameters) return;

	deviceParameters->b_digital = (passthrough || m_bEncodeAC3) && g_audioConfig.HasDigitalOutput();
	deviceParameters->i_hog_pid = -1;
	deviceParameters->i_stream_index = -1;
	
	m_dwPacketSize = packetSize;
	
	m_bIsMusic = isMusic;

	// Build a list of devices.
	deviceArray = PlexAudioDevices::FindAll();

	i_param_size = sizeof(deviceParameters->i_hog_pid);
	err = AudioDeviceGetProperty( deviceArray->getSelectedDevice()->getDeviceID(), 0, FALSE, kAudioDevicePropertyHogMode, &i_param_size, &deviceParameters->i_hog_pid );
	if( err != noErr )
	{
	  /* This is not a fatal error. Some drivers simply don't support this property */
	  CLog::Log(LOGINFO, "could not check whether device is hogged: %4.4s", (char *)&err );
	  deviceParameters->i_hog_pid = -1;
	}

	if( deviceParameters->i_hog_pid != -1 && deviceParameters->i_hog_pid != getpid() )
	{
		CLog::Log(LOGERROR, "Selected audio device is exclusively in use by another program.");
		return;
	}

	deviceParameters->device_id = deviceArray->getSelectedDevice()->getDeviceID();

	/* Check for Digital mode or Analog output mode */
	if (deviceParameters->b_digital)
	{
	  if (OpenSPDIF(deviceParameters, strName, channels, sampleRate, bitsPerSample, packetSize))
	  {
		m_dwPacketSize = AC3_SPDIF_FRAME_SIZE;
	    m_bIsInitialized = true;
	    return;
	  }
	}
	else if (g_audioConfig.ForcedDigital() && (m_bEncodeAC3 || passthrough))
	{
		if (OpenPCM(deviceParameters, strName, SPDIF_CHANNELS, SPDIF_SAMPLERATE, SPDIF_SAMPLESIZE, packetSize))
		{
			m_bIsInitialized = true;
			return;
		}
	}
	else
	{
	  if (OpenPCM(deviceParameters, strName, channels, sampleRate, bitsPerSample, packetSize))
	  {
	    m_bIsInitialized = true;
	    return;
	  }
	}

error:
  /* If we reach this, this aout has failed */
  //var_Destroy( p_aout, "audio-device" );
  free(deviceParameters);
	m_bIsInitialized = false;
  //return VLC_EGENERIC;
  return;
}

HRESULT CoreAudioAUHAL::Deinitialize()
{
  // Don't allow double deinitialization.
  if (m_bIsInitialized == false)
    return S_OK;
  
	CLog::Log(LOGDEBUG,"CoreAudioAUHAL::Deinitialize");
	
	OSStatus            err = noErr;
    UInt32              i_param_size = 0;
	
    if(deviceParameters->au_unit)
    {
        verify_noerr( AudioOutputUnitStop( deviceParameters->au_unit ) );
        verify_noerr( AudioUnitUninitialize( deviceParameters->au_unit ) );
        verify_noerr( CloseComponent( deviceParameters->au_unit ) );
		deviceParameters->au_unit = NULL;
    }
	
	if (m_bEncodeAC3)
	{
		ac3encoder_free(&m_ac3encoder);
	}
	if (ac3_framebuffer != NULL)
	{
		free(ac3_framebuffer);
		ac3_framebuffer = NULL;
	}
	
    if( deviceParameters->b_digital )
    {
        /* Stop device */
        err = AudioDeviceStop( deviceParameters->device_id,
							  (AudioDeviceIOProc)RenderCallbackSPDIF );
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "AudioDeviceStop failed: [%4.4s]", (char *)&err );
        }
		
        /* Remove IOProc callback */
		err = AudioDeviceDestroyIOProcID(deviceParameters->device_id, deviceParameters->sInputIOProcID);
		
        if( err != noErr )
        {
            CLog::Log(LOGERROR, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
		
        if( deviceParameters->b_changed_mixing && deviceParameters->sfmt_revert.mFormatID != kAudioFormat60958AC3 )
        {
            int b_mix;
            Boolean b_writeable = false;
            /* Revert mixable to true if we are allowed to */
            err = AudioDeviceGetPropertyInfo(deviceParameters->device_id, 0, FALSE, kAudioDevicePropertySupportsMixing,
											 &i_param_size, &b_writeable );
			
            err = AudioDeviceGetProperty( deviceParameters->device_id, 0, FALSE, kAudioDevicePropertySupportsMixing,
										 &i_param_size, &b_mix );
			
            if( !err && b_writeable )
            {
                CLog::Log(LOGDEBUG, "mixable is: %d", b_mix );
                b_mix = 1;
                err = AudioDeviceSetProperty( deviceParameters->device_id, 0, 0, FALSE,
											 kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
            }
			
            if( err != noErr )
            {
                CLog::Log(LOGERROR, "failed to set mixmode: [%4.4s]", (char *)&err );
            }
        }
    }
#warning fix listener
    //err = AudioHardwareRemovePropertyListener( kAudioHardwarePropertyDevices,
	//										  HardwareListener );
	
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "AudioHardwareRemovePropertyListener failed: [%4.4s]", (char *)&err );
    }
	
    if( deviceParameters->i_hog_pid == getpid() )
    {
        deviceParameters->i_hog_pid = -1;
        i_param_size = sizeof( deviceParameters->i_hog_pid );
        err = AudioDeviceSetProperty( deviceParameters->device_id, 0, 0, FALSE,
									 kAudioDevicePropertyHogMode, i_param_size, &deviceParameters->i_hog_pid );
        if( err != noErr ) CLog::Log(LOGERROR, "Could not release hogmode: [%4.4s]", (char *)&err );
    }

    // Revert the stream format *after* we've set all the parameters, as doing it before seems to 
    // result in a hang under some circumstances, an apparent deadlock in CoreAudio between handing
    // the stream format change and setting parameters. 
    //
    if (deviceParameters->b_digital && deviceParameters->b_revert)
      AudioStreamChangeFormat(deviceParameters, deviceParameters->i_stream_id, deviceParameters->sfmt_revert);
   
  m_bIsInitialized = false;
	return S_OK;
}

DWORD CoreAudioAUHAL::GetSpace()
{
	DWORD fakeCeiling, bufferDataSize = PaUtil_GetRingBufferReadAvailable(deviceParameters->outputBuffer);
	
	if (m_bIsMusic)
	{
		fakeCeiling = PACKET_SIZE / deviceParameters->stream_format.mChannelsPerFrame;
	}
	else
	{
		fakeCeiling = deviceParameters->stream_format.mSampleRate * CA_BUFFER_FACTOR;
	}
	
	if (bufferDataSize < fakeCeiling)
	{
		return fakeCeiling - bufferDataSize;
	}
	else
	{
		return 0;
	}
}

float CoreAudioAUHAL::GetHardwareLatency()
{
	float latency = CA_BUFFER_FACTOR + ((float)deviceParameters->hardwareFrameLatency / deviceParameters->stream_format.mSampleRate);
	if (deviceParameters->b_digital)
		latency += 0.032;
	return latency;
}

AudioStreamBasicDescription* CoreAudioAUHAL::GetStreamDescription()
{
	return &deviceParameters->stream_format;
};

int CoreAudioAUHAL::WriteStream(uint8_t *sampleBuffer, uint32_t samplesToWrite)
{
	
	if (sampleBuffer == NULL || samplesToWrite == 0)
	{
		return 0;
	}
	
	int inputByteFactor, outputByteFactor;
	
	if (m_bEncodeAC3) // use the raw PCM channel count to get the number of samples to play
	{
		inputByteFactor = ac3encoder_channelcount(&m_ac3encoder) * m_uiBitsPerSample/8;
		outputByteFactor = SPDIF_SAMPLE_BYTES;
	}
	else // the PCM input and stream output should match
	{
		inputByteFactor = outputByteFactor = deviceParameters->stream_format.mBytesPerFrame;
	}
	
	
	if (m_bEncodeAC3)
	{
		int ac3_frame_count = 0;
		
		if ((ac3_frame_count = ac3encoder_write_samples(&m_ac3encoder, sampleBuffer, samplesToWrite)) == 0)
		{
			CLog::Log(LOGERROR, "AC3 output buffer underrun");
			return 0;
		}
		else
		{
			int buffer_sample_readcount = -1;
			if ((buffer_sample_readcount = ac3encoder_get_encoded_samples(&m_ac3encoder, ac3_framebuffer, samplesToWrite)) != samplesToWrite)
			{
				CLog::Log(LOGERROR, "AC3 output buffer underrun");
			}
			else
			{
				return PaUtil_WriteRingBuffer(deviceParameters->outputBuffer, ac3_framebuffer, samplesToWrite);
			}
		}
	}
	else
	{
		return PaUtil_WriteRingBuffer(deviceParameters->outputBuffer, sampleBuffer, samplesToWrite);
	}
	return 0;
}

void CoreAudioAUHAL::Flush()
{
	if (m_bEncodeAC3)
	{
	//CSingleLock lock(m_cs); // acquire lock
	
	//PaUtil_FlushRingBuffer( deviceParameters->outputBuffer );
	if (m_bEncodeAC3)
	{
	//	ac3encoder_flush(&m_ac3encoder);
	}
}

#pragma mark Analog (PCM)

/*****************************************************************************
 * Open: open and setup a HAL AudioUnit to do analog (multichannel) audio output
 *****************************************************************************/
int CoreAudioAUHAL::OpenPCM(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, float sampleRate, int bitsPerSample, int packetSize)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    ComponentDescription        desc;
    AudioStreamBasicDescription DeviceFormat;
    AURenderCallbackStruct      input;

    // We're non-digital.
    s_lastPlayWasSpdif = false;
    
    /* Lets go find our Component */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    deviceParameters->au_component = FindNextComponent( NULL, &desc );
    if(deviceParameters->au_component == NULL)
    {
		CLog::Log(LOGERROR, "we cannot find our HAL component");
        return false;
    }

    err = OpenAComponent( deviceParameters->au_component, &deviceParameters->au_unit );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "we cannot open our HAL component");
        return false;
    }

    AudioDeviceID selectedDeviceID = deviceArray->getSelectedDevice()->getDeviceID();
    
    /* Set the device we will use for this output unit */
    err = AudioUnitSetProperty(deviceParameters->au_unit,
							   kAudioOutputUnitProperty_CurrentDevice,
							   kAudioUnitScope_Input,
							   0,
							   &selectedDeviceID,
							   sizeof(AudioDeviceID));

    if( err != noErr )
    {
		CLog::Log(LOGERROR, "we cannot select the audio device");
        return false;
    }

    /* Get the current format */
    i_param_size = sizeof(AudioStreamBasicDescription);

    err = AudioUnitGetProperty(deviceParameters->au_unit,
							   kAudioUnitProperty_StreamFormat,
							   kAudioUnitScope_Input,
							   0,
							   &deviceParameters->sfmt_revert,
							   &i_param_size );

    if( err != noErr ) return false;
    else CLog::Log(LOGINFO, STREAM_FORMAT_MSG("current format is: ", deviceParameters->sfmt_revert) );

    /* Set up the format to be used */
    DeviceFormat.mSampleRate = sampleRate;
    DeviceFormat.mFormatID = kAudioFormatLinearPCM;
    DeviceFormat.mFormatFlags = (bitsPerSample == 32 ? kLinearPCMFormatFlagIsFloat : kLinearPCMFormatFlagIsSignedInteger);
    DeviceFormat.mBitsPerChannel = bitsPerSample;
    DeviceFormat.mChannelsPerFrame = channels;

    /* Calculate framesizes and stuff */
    DeviceFormat.mFramesPerPacket = 1;
    DeviceFormat.mBytesPerFrame = DeviceFormat.mBitsPerChannel/8 * DeviceFormat.mChannelsPerFrame;
    DeviceFormat.mBytesPerPacket = DeviceFormat.mBytesPerFrame * DeviceFormat.mFramesPerPacket;

    /* Set the desired format */
    i_param_size = sizeof(AudioStreamBasicDescription);
    verify_noerr( AudioUnitSetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Input,
									   0,
									   &DeviceFormat,
									   i_param_size ));

	CLog::Log(LOGINFO, STREAM_FORMAT_MSG( "we set the AU format: " , DeviceFormat ) );

    /* Retrieve actual format */
    verify_noerr( AudioUnitGetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Input,
									   0,
									   &deviceParameters->stream_format,
									   &i_param_size ));

	CLog::Log(LOGINFO, STREAM_FORMAT_MSG( "the actual set AU format is " , DeviceFormat ) );

    /* set the IOproc callback */
	input.inputProc = (AURenderCallback) RenderCallbackAnalog;
	input.inputProcRefCon = deviceParameters;

    verify_noerr( AudioUnitSetProperty(deviceParameters->au_unit,
									   kAudioUnitProperty_SetRenderCallback,
									   kAudioUnitScope_Global,
									   0, &input, sizeof(input)));

    /* AU initialize */
    verify_noerr( AudioUnitInitialize(deviceParameters->au_unit));

	// Get AU hardware buffer size

	uint32_t audioDeviceLatency, audioDeviceBufferFrameSize, audioDeviceSafetyOffset;
	deviceParameters->hardwareFrameLatency = 0;
	
	i_param_size = sizeof(uint32_t);

	verify_noerr( AudioUnitGetProperty(deviceParameters->au_unit,
									   kAudioDevicePropertyLatency,
									   kAudioUnitScope_Global,
									   0,
									   &audioDeviceLatency,
									   &i_param_size ));

	deviceParameters->hardwareFrameLatency += audioDeviceLatency;

	verify_noerr( AudioUnitGetProperty(deviceParameters->au_unit,
									   kAudioDevicePropertyBufferFrameSize,
									   kAudioUnitScope_Global,
									   0,
									   &audioDeviceBufferFrameSize,
									   &i_param_size ));

	deviceParameters->hardwareFrameLatency += audioDeviceBufferFrameSize;

	verify_noerr( AudioUnitGetProperty(deviceParameters->au_unit,
									   kAudioDevicePropertySafetyOffset,
									   kAudioUnitScope_Global,
									   0,
									   &audioDeviceSafetyOffset,
									   &i_param_size ));
	
	deviceParameters->hardwareFrameLatency += audioDeviceSafetyOffset;

	CLog::Log(LOGINFO, "Hardware latency: %i frames (%.2f msec @ %.0fHz)", deviceParameters->hardwareFrameLatency,
			  (float)deviceParameters->hardwareFrameLatency / deviceParameters->stream_format.mSampleRate * 1000,
			  deviceParameters->stream_format.mSampleRate);

	// initialise the CoreAudio sink buffer
	uint32_t framecount = 1;
	while(framecount <= deviceParameters->stream_format.mSampleRate) // ensure power of 2
	{
		framecount <<= 1;
	}
	deviceParameters->outputBuffer = (PaUtilRingBuffer *)malloc(sizeof(PaUtilRingBuffer));
	deviceParameters->outputBufferData = malloc(framecount * deviceParameters->stream_format.mBytesPerFrame);

	PaUtil_InitializeRingBuffer(deviceParameters->outputBuffer,
								deviceParameters->stream_format.mBytesPerFrame,
								framecount, deviceParameters->outputBufferData);


    /* Start the AU */
    verify_noerr( AudioOutputUnitStart(deviceParameters->au_unit) );

    return true;
}

/*****************************************************************************
 * RenderCallbackAnalog: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
OSStatus CoreAudioAUHAL::RenderCallbackAnalog(struct CoreAudioDeviceParameters *deviceParameters,
									  int *ioActionFlags,
									  const AudioTimeStamp *inTimeStamp,
									  unsigned int inBusNumber,
									  unsigned int inNumberFrames,
									  AudioBufferList *ioData )
{
    // initial calc
	int framesToWrite = inNumberFrames;
	int framesAvailable = PaUtil_GetRingBufferReadAvailable(deviceParameters->outputBuffer);
	
	if (framesToWrite > framesAvailable)
	{
		framesToWrite = framesAvailable;
	}
	
	int currentPos = framesToWrite * deviceParameters->stream_format.mBytesPerFrame;
	int underrunLength = (inNumberFrames - framesToWrite) * deviceParameters->stream_format.mBytesPerFrame;

	// write as many frames as possible from buffer
	PaUtil_ReadRingBuffer(deviceParameters->outputBuffer, ioData->mBuffers[0].mData, framesToWrite);
	// write silence to any remainder
	if (underrunLength > 0)
	{
		memset((void *)((uint8_t *)(ioData->mBuffers[0].mData)+currentPos), 0, underrunLength);
	}

    return( noErr );
}

#pragma mark Digital (SPDIF)

/*****************************************************************************
 * Setup a encoded digital stream (SPDIF)
 *****************************************************************************/
int CoreAudioAUHAL::OpenSPDIF(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, float sampleRate, int bitsPerSample, int packetSize)

{
	OSStatus                err = noErr;
    UInt32                  i_param_size = 0, b_mix = 0;
    Boolean                 b_writeable = false;
    AudioStreamID           *p_streams = NULL;
    int                     i = 0, i_streams = 0;

    // We're digital.
    s_lastPlayWasSpdif = true;
    
    /* Start doing the SPDIF setup proces */
    //deviceParameters->b_digital = true;
	deviceParameters->b_changed_mixing = false;

    /* Hog the device */
    i_param_size = sizeof(deviceParameters->i_hog_pid);
    deviceParameters->i_hog_pid = getpid();

    err = AudioDeviceSetProperty(deviceParameters->device_id, 0, 0, FALSE,
								 kAudioDevicePropertyHogMode, i_param_size, &deviceParameters->i_hog_pid);

    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Failed to set hogmode: [%4.4s]", (char *)&err );
        return false;
    }

    /* Set mixable to false if we are allowed to */
    err = AudioDeviceGetPropertyInfo(deviceParameters->device_id, 0, FALSE, kAudioDevicePropertySupportsMixing,
									 &i_param_size, &b_writeable );

    err = AudioDeviceGetProperty(deviceParameters->device_id, 0, FALSE, kAudioDevicePropertySupportsMixing,
								 &i_param_size, &b_mix );

    if( !err && b_writeable )
    {
        b_mix = 0;
        err = AudioDeviceSetProperty( deviceParameters->device_id, 0, 0, FALSE,
									 kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
        deviceParameters->b_changed_mixing = true;
    }

    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Failed to set mixmode: [%4.4s]", (char *)&err );
        return false;
    }

    /* Get a list of all the streams on this device */
    err = AudioDeviceGetPropertyInfo(deviceParameters->device_id, 0, FALSE,
									 kAudioDevicePropertyStreams,
									 &i_param_size, NULL );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Could not get number of streams: [%4.4s]", (char *)&err );
        return false;
    }

    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
        return false;

    err = AudioDeviceGetProperty(deviceParameters->device_id, 0, FALSE,
								 kAudioDevicePropertyStreams,
								 &i_param_size, p_streams );

    if( err != noErr )
    {
		CLog::Log(LOGERROR, "Could not get number of streams: [%4.4s]", (char *)&err );
        free( p_streams );
        return false;
    }

    for( i = 0; i < i_streams && deviceParameters->i_stream_index < 0 ; i++ )
    {
        /* Find a stream with a cac3 stream */
        AudioStreamBasicDescription *p_format_list = NULL;
        int                         i_formats = 0, j = 0;
        bool                  b_digital = false;

        /* Retrieve all the stream formats supported by each output stream */
        err = AudioStreamGetPropertyInfo( p_streams[i], 0,
										 kAudioStreamPropertyPhysicalFormats,
										 &i_param_size, NULL );
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "Could not get number of streamformats: [%4.4s]", (char *)&err );
            continue;
        }

        i_formats = i_param_size / sizeof( AudioStreamBasicDescription );
        p_format_list = (AudioStreamBasicDescription *)malloc( i_param_size );
        if( p_format_list == NULL )
            continue;

        err = AudioStreamGetProperty( p_streams[i], 0,
									 kAudioStreamPropertyPhysicalFormats,
									 &i_param_size, p_format_list );
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "Could not get the list of streamformats: [%4.4s]", (char *)&err );
            free( p_format_list );
            continue;
        }

        /* Check if one of the supported formats is a digital format */
        for( j = 0; j < i_formats; j++ )
        {
            if( p_format_list[j].mFormatID == 'IAC3' ||
			   p_format_list[j].mFormatID == kAudioFormat60958AC3 )
            {
                b_digital = true;
                break;
            }
        }

        if( b_digital )
        {
            /* if this stream supports a digital (cac3) format, then go set it. */
            int i_requested_rate_format = -1;
            int i_current_rate_format = -1;
            int i_backup_rate_format = -1;

            deviceParameters->i_stream_id = p_streams[i];
            deviceParameters->i_stream_index = i;

            if(deviceParameters->b_revert == false )
            {
                /* Retrieve the original format of this stream first if not done so already */
                i_param_size = sizeof(deviceParameters->sfmt_revert);
                err = AudioStreamGetProperty(deviceParameters->i_stream_id, 0,
											 kAudioStreamPropertyPhysicalFormat,
											 &i_param_size,
											 &deviceParameters->sfmt_revert );
                if( err != noErr )
                {
					CLog::Log(LOGERROR, "Could not retrieve the original streamformat: [%4.4s]", (char *)&err );
                    //continue;
                }
                else deviceParameters->b_revert = true;
            }

            for( j = 0; j < i_formats; j++ )
            {
                if( p_format_list[j].mFormatID == 'IAC3' ||
				   p_format_list[j].mFormatID == kAudioFormat60958AC3 )
                {
                    if( p_format_list[j].mSampleRate == sampleRate)
                    {
                        i_requested_rate_format = j;
                        break;
                    }
                    else if( p_format_list[j].mSampleRate == deviceParameters->sfmt_revert.mSampleRate )
                    {
                        i_current_rate_format = j;
                    }
                    else
                    {
                        if( i_backup_rate_format < 0 || p_format_list[j].mSampleRate > p_format_list[i_backup_rate_format].mSampleRate )
                            i_backup_rate_format = j;
                    }
                }

            }

            if( i_requested_rate_format >= 0 ) /* We prefer to output at the samplerate of the original audio */
                deviceParameters->stream_format = p_format_list[i_requested_rate_format];
            else if( i_current_rate_format >= 0 ) /* If not possible, we will try to use the current samplerate of the device */
                deviceParameters->stream_format = p_format_list[i_current_rate_format];
            else deviceParameters->stream_format = p_format_list[i_backup_rate_format]; /* And if we have to, any digital format will be just fine (highest rate possible) */
        }
        free( p_format_list );
    }
    free( p_streams );
	
	CLog::Log(LOGINFO, STREAM_FORMAT_MSG("original stream format: ", deviceParameters->sfmt_revert ) );

    if( !AudioStreamChangeFormat(deviceParameters, deviceParameters->i_stream_id, deviceParameters->stream_format))
        return false;

	// Get device hardware buffer size

	uint32_t audioDeviceLatency, audioStreamLatency, audioDeviceBufferFrameSize, audioDeviceSafetyOffset;
	deviceParameters->hardwareFrameLatency = 0;
	i_param_size = sizeof(uint32_t);

	err = AudioDeviceGetProperty(deviceParameters->device_id,
						   0, false,
						   kAudioDevicePropertyLatency,
						   &i_param_size,
						   &audioDeviceLatency);

	if (err == noErr) deviceParameters->hardwareFrameLatency += audioDeviceLatency;

	err = AudioDeviceGetProperty(deviceParameters->device_id,
						   0, false,
						   kAudioDevicePropertyBufferFrameSize,
						   &i_param_size,
						   &audioDeviceBufferFrameSize);

	if (err == noErr) deviceParameters->hardwareFrameLatency += audioDeviceBufferFrameSize;

	err = AudioDeviceGetProperty(deviceParameters->device_id,
						   0, false,
						   kAudioDevicePropertySafetyOffset,
						   &i_param_size,
						   &audioDeviceSafetyOffset);

	if (err == noErr) deviceParameters->hardwareFrameLatency += audioDeviceSafetyOffset;

	err = AudioStreamGetProperty(deviceParameters->i_stream_id,
						   0,
						   kAudioStreamPropertyLatency,
						   &i_param_size,
						   &audioStreamLatency);

	if (err == noErr) deviceParameters->hardwareFrameLatency += audioStreamLatency;


	CLog::Log(LOGINFO, "Hardware latency: %i frames (%.2f msec @ %.0fHz)", deviceParameters->hardwareFrameLatency,
			  (float)deviceParameters->hardwareFrameLatency / deviceParameters->stream_format.mSampleRate * 1000,
			  deviceParameters->stream_format.mSampleRate);

  	// initialise the CoreAudio sink buffer
	uint32_t framecount = 1;
	while(framecount <= deviceParameters->stream_format.mSampleRate) // ensure power of 2
	{
		framecount <<= 1;
	}

#warning free
	deviceParameters->outputBuffer = (PaUtilRingBuffer *)malloc(sizeof(PaUtilRingBuffer));
	deviceParameters->outputBufferData = malloc(framecount * SPDIF_SAMPLE_BYTES);

	PaUtil_InitializeRingBuffer(deviceParameters->outputBuffer, SPDIF_SAMPLE_BYTES, framecount, deviceParameters->outputBufferData);
	/* Add IOProc callback */
	err = AudioDeviceCreateIOProcID(deviceParameters->device_id,
									(AudioDeviceIOProc)RenderCallbackSPDIF,
									deviceParameters,
									&deviceParameters->sInputIOProcID);
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "AudioDeviceAddIOProcID failed: [%4.4s]", (char *)&err );
        return false;
    }

    /* Start device */
    err = AudioDeviceStart(deviceParameters->device_id, (AudioDeviceIOProc)RenderCallbackSPDIF );
    if( err != noErr )
    {
		CLog::Log(LOGERROR, "AudioDeviceStart failed: [%4.4s]", (char *)&err );

        err = AudioDeviceDestroyIOProcID(deviceParameters->device_id,
										 (AudioDeviceIOProc)RenderCallbackSPDIF);
        if( err != noErr )
        {
			CLog::Log(LOGERROR, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
        return false;
    }

    return true;
}

/*****************************************************************************
 * AudioStreamChangeFormat: Change i_stream_id to change_format
 *****************************************************************************/
int CoreAudioAUHAL::AudioStreamChangeFormat(CoreAudioDeviceParameters *deviceParameters, AudioStreamID i_stream_id, AudioStreamBasicDescription change_format)
{
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;
    int i;

    CLog::Log(LOGINFO, STREAM_FORMAT_MSG( "setting stream format: ", change_format ));
	
	CSingleLock lock(m_cs); // acquire lock

    /* change the format */
    err = AudioStreamSetProperty( i_stream_id, 0, 0,
								 kAudioStreamPropertyPhysicalFormat,
								 sizeof( AudioStreamBasicDescription ),
								 &change_format );
    if( err != noErr )
    {
        CLog::Log(LOGERROR, "could not set the stream format: [%4.4s]", (char *)&err );
        return false;
    }

    /* The AudioStreamSetProperty is not only asynchronious (requiring the locks)
     * it is also not atomic in its behaviour.
     * Therefore we check 5 times before we really give up.*/
    for( i = 0; i < 5; i++ )
    {
        AudioStreamBasicDescription actual_format;
		usleep(20);
        i_param_size = sizeof( AudioStreamBasicDescription );
        err = AudioStreamGetProperty( i_stream_id, 0,
									 kAudioStreamPropertyPhysicalFormat,
									 &i_param_size,
									 &actual_format );

        CLog::Log(LOGDEBUG, STREAM_FORMAT_MSG( "actual format in use: ", actual_format ) );
        if( actual_format.mSampleRate == change_format.mSampleRate &&
		   actual_format.mFormatID == change_format.mFormatID &&
		   actual_format.mFramesPerPacket == change_format.mFramesPerPacket )
        {
            /* The right format is now active */
            break;
        }
        /* We need to check again */
    }

    return true;
}

/*****************************************************************************
 * RenderCallbackSPDIF: callback for SPDIF audio output
 *****************************************************************************/
OSStatus CoreAudioAUHAL::RenderCallbackSPDIF(AudioDeviceID inDevice,
                                    const AudioTimeStamp * inNow,
                                    const void * inInputData,
                                    const AudioTimeStamp * inInputTime,
                                    AudioBufferList * outOutputData,
                                    const AudioTimeStamp * inOutputTime,
                                    void * threadGlobals )
{
    CoreAudioDeviceParameters *deviceParameters = (CoreAudioDeviceParameters *)threadGlobals;

#define BUFFER outOutputData->mBuffers[deviceParameters->i_stream_index]
	int framesToWrite = BUFFER.mDataByteSize / deviceParameters->outputBuffer->elementSizeBytes;

	if (framesToWrite > PaUtil_GetRingBufferReadAvailable(deviceParameters->outputBuffer))
	{
		// we can't write a frame, send null frame
        memset(BUFFER.mData, 0, BUFFER.mDataByteSize);
    }
	else
	{
		// write a frame
		PaUtil_ReadRingBuffer(deviceParameters->outputBuffer, BUFFER.mData, framesToWrite);
	}
#undef BUFFER
    return( noErr );
}

#endif

