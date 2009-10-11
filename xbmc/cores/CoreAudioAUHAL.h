/*
 *  CoreAudioAUHAL.h
 *  Plex
 *
 *  Created by Ryan Walklin on 9/6/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

/*
 * XBoxMediaPlayer
 * Copyright (c) 2002 d7o3g4q and RUNTiME
 * Portions Copyright (c) by the authors of ffmpeg and xvid
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#ifndef __COREAUDIO_AUHAL_H__
#define __COREAUDIO_AUHAL_H__

#include "pa_ringbuffer.h"
#include "CoreAudioPlexSupport.h"
#include "stdafx.h"
#include "XBAudioConfig.h"
#include "utils/CriticalSection.h"

extern "C" {
#include "ac3encoder.h"
};

#define CA_BUFFER_FACTOR 0.05

class CoreAudioAUHAL
	{
	public:
		CoreAudioAUHAL(const CStdString& strName, 
									   const char *strCodec, 
									   int channels, 
									   unsigned int sampleRate, 
									   int bitsPerSample, 
									   bool passthrough, 
									   bool isMusic, 
									   int packetSize);
		bool IsInitialized() { return m_bIsInitialized; }
		virtual HRESULT Deinitialize();
		virtual DWORD GetSpace();
		virtual float GetHardwareLatency();
		virtual AudioStreamBasicDescription* GetStreamDescription();
		virtual int WriteStream(uint8_t *sampleBuffer, uint32_t samplesToWrite);
		virtual void Flush();
		int PacketSize() { return m_dwPacketSize; }

		// Addpackets - make ringbuffer private

		static bool LastOpenWasSpdif() { bool ret = s_lastPlayWasSpdif; s_lastPlayWasSpdif = false; return ret; }
		
	private:
		virtual int OpenPCM(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, float sampleRate, int bitsPerSample, int packetSize);
		static OSStatus RenderCallbackAnalog(struct CoreAudioDeviceParameters *deviceParameters,
											 int *ioActionFlags,
											 const AudioTimeStamp *inTimeStamp,
											 unsigned int inBusNummer,
											 unsigned int inNumberFrames,
											 AudioBufferList *ioData );
		virtual int OpenSPDIF(struct CoreAudioDeviceParameters *deviceParameters, const CStdString& strName, int channels, float sampleRate, int bitsPerSample, int packetSize);
		virtual int AudioStreamChangeFormat(CoreAudioDeviceParameters *deviceParameters, AudioStreamID i_stream_id, AudioStreamBasicDescription change_format);
		static OSStatus RenderCallbackSPDIF(AudioDeviceID inDevice,
											const AudioTimeStamp * inNow,
											const void * inInputData,
											const AudioTimeStamp * inInputTime,
											AudioBufferList * outOutputData,
											const AudioTimeStamp * inOutputTime,
											void * threadGlobals );
		static OSStatus HardwareStreamListener(AudioObjectID inObjectID,
										UInt32        inNumberAddresses,
										const AudioObjectPropertyAddress inAddresses[],
										void* inClientData);
		
		static PlexAudioDevicesPtr GetDeviceArray(); // could make this public interface to current device
		
		PlexAudioDevicesPtr deviceArray;
		struct CoreAudioDeviceParameters* deviceParameters;
		
		bool m_bIsMusic;
		bool m_bIsInitialized;
		CCriticalSection m_cs;
		
		int m_uiChannels;
		int m_uiBitsPerSample;
		int m_uiSamplesPerSec;
		
		int m_dwPacketSize;
		
		bool m_bEncodeAC3;
		AC3Encoder m_ac3encoder;
		
		static bool s_lastPlayWasSpdif;
	};
		
#endif

