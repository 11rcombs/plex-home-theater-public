@ECHO OFF
rem ----Usage----
rem BuildSetup [vs2008|vs2010] [gl|dx] [clean|noclean]
rem vs2010 for compiling with visual studio 2010
rem gl for opengl build (default)
rem dx for directx build
rem clean to force a full rebuild
rem noclean to force a build without clean
rem noprompt to avoid all prompts
CLS
COLOR 1B
TITLE Plex for Windows Build Script
rem ----PURPOSE----
rem - Create a working Plex build with a single click
rem -------------------------------------------------------------
rem Config
rem If you get an error that Visual studio was not found, SET your path for VSNET main executable.
rem -------------------------------------------------------------
rem	CONFIG START
SET comp=vs2010
SET target=dx
SET buildmode=ask
SET promptlevel=prompt
FOR %%b in (%1, %2, %3, %4, %5) DO (
  SET comp=vs2010
	IF %%b==dx SET target=dx
	IF %%b==gl SET target=gl
	IF %%b==clean SET buildmode=clean
	IF %%b==noclean SET buildmode=noclean
	IF %%b==noprompt SET promptlevel=noprompt
)
SET buildconfig=Release (OpenGL)
IF %target%==dx SET buildconfig=Release (DirectX)

IF "%VS100COMNTOOLS%"=="" (
	set NET="%ProgramFiles%\Microsoft Visual Studio 10.0\Common7\IDE\VCExpress.exe"
) ELSE IF EXIST "%VS100COMNTOOLS%\..\IDE\VCExpress.exe" (
	set NET="%VS100COMNTOOLS%\..\IDE\VCExpress.exe"
) ELSE IF EXIST "%VS100COMNTOOLS%\..\IDE\devenv.exe" (
	set NET="%VS100COMNTOOLS%\..\IDE\devenv.exe"
)

IF NOT EXIST %NET% (
	IF %comp%==vs2010 (
	  set DIETEXT=Visual Studio .NET 2010 Express was not found.
	)
	goto DIE
)
  
set OPTS_EXE="..\VS2010Express\Plex for Windows.sln" /build "%buildconfig%"
set CLEAN_EXE="..\VS2010Express\Plex for Windows.sln" /clean "%buildconfig%"
set EXE= "..\VS2010Express\Plex\%buildconfig%\Plex.exe"

	
  rem	CONFIG END
  rem -------------------------------------------------------------
 
  goto EXE_COMPILE

:EXE_COMPILE
  IF %buildmode%==clean goto COMPILE_EXE
  rem ---------------------------------------------
  rem	check for existing exe
  rem ---------------------------------------------
  IF EXIST buildlog.html del buildlog.html /q
  
  IF EXIST %EXE% (
    goto EXE_EXIST
  )
  goto COMPILE_EXE

:EXE_EXIST
  IF %buildmode%==noclean goto COMPILE_NO_CLEAN_EXE
  ECHO ------------------------------------------------------------
  ECHO Found a previous Compiled WIN32 EXE!
  ECHO [1] a NEW EXE will be compiled for the BUILD_WIN32
  ECHO [2] existing EXE will be updated (quick mode compile) for the BUILD_WIN32
  ECHO ------------------------------------------------------------
  set /P PLEX_COMPILE_ANSWER=Compile a new EXE? [1/2]:
  if /I %PLEX_COMPILE_ANSWER% EQU 1 goto COMPILE_EXE
  if /I %PLEX_COMPILE_ANSWER% EQU 2 goto COMPILE_NO_CLEAN_EXE
  
:COMPILE_EXE
  ECHO Wait while preparing the build.
  ECHO ------------------------------------------------------------
  ECHO Cleaning Solution...
  %NET% %CLEAN_EXE%
  ECHO Compiling Plex...
  %NET% %OPTS_EXE%
  IF NOT EXIST %EXE% (
  	set DIETEXT="Plex.exe failed to build!  See ..\vs2010express\Plex\%buildconfig%\BuildLog.htm for details."
  	goto DIE
  )
  ECHO Done!
  ECHO ------------------------------------------------------------
  GOTO MAKE_BUILD_EXE
  
:COMPILE_NO_CLEAN_EXE
  ECHO Wait while preparing the build.
  ECHO ------------------------------------------------------------
  ECHO Compiling Solution...
  %NET% %OPTS_EXE%
  IF NOT EXIST %EXE% (
  	set DIETEXT="Plex.exe failed to build!  See ..\vs2010express\Plex\%buildconfig%\BuildLog\BuildLog.htm for details."
  	goto DIE
  )
  ECHO Done!
  ECHO ------------------------------------------------------------
  GOTO MAKE_BUILD_EXE

:MAKE_BUILD_EXE
  ECHO Copying files...
  IF EXIST BUILD_WIN32 rmdir BUILD_WIN32 /S /Q

  Echo .svn>exclude.txt
  Echo CVS>>exclude.txt
  Echo .so>>exclude.txt
  Echo Thumbs.db>>exclude.txt
  Echo Desktop.ini>>exclude.txt
  Echo dsstdfx.bin>>exclude.txt
  Echo exclude.txt>>exclude.txt
  rem and exclude potential leftovers
  Echo mediasources.xml>>exclude.txt
  Echo advancedsettings.xml>>exclude.txt
  Echo guisettings.xml>>exclude.txt
  Echo profiles.xml>>exclude.txt
  Echo sources.xml>>exclude.txt
  Echo userdata\cache\>>exclude.txt
  Echo userdata\database\>>exclude.txt
  Echo userdata\playlists\>>exclude.txt
  Echo userdata\script_data\>>exclude.txt
  Echo userdata\thumbnails\>>exclude.txt
  rem UserData\visualisations contains currently only xbox visualisationfiles
  Echo userdata\visualisations\>>exclude.txt
  rem other platform stuff
  Echo lib-osx>>exclude.txt
  Echo players\mplayer>>exclude.txt
  Echo FileZilla Server.xml>>exclude.txt
  Echo asound.conf>>exclude.txt
  Echo voicemasks.xml>>exclude.txt
  Echo Lircmap.xml>>exclude.txt
  
  md BUILD_WIN32\Plex

  xcopy %EXE% BUILD_WIN32\Plex > NUL
  xcopy ..\..\userdata BUILD_WIN32\Plex\userdata /E /Q /I /Y /EXCLUDE:exclude.txt > NUL
  copy ..\..\copying.txt BUILD_WIN32\Plex > NUL
  copy ..\..\LICENSE.GPL BUILD_WIN32\Plex > NUL
  copy ..\..\known_issues.txt BUILD_WIN32\Plex > NUL
  copy ..\..\Plex\Windows\winsparkle\WinSparkle-0.3\WinSparkle.dll  BUILD_WIN32\Plex > NUL
  xcopy dependencies\*.* BUILD_WIN32\Plex /Q /I /Y /EXCLUDE:exclude.txt  > NUL
  xcopy vs_redistributable\vs2010\vcredist_x86.exe BUILD_WIN32\Plex /Q /I /Y /EXCLUDE:exclude.txt  > NUL
  copy sources.xml BUILD_WIN32\Plex\userdata > NUL
  
  xcopy ..\..\language BUILD_WIN32\Plex\language /E /Q /I /Y /EXCLUDE:exclude.txt  > NUL
  xcopy ..\..\addons BUILD_WIN32\Plex\addons /E /Q /I /Y /EXCLUDE:exclude.txt > NUL
  xcopy ..\..\system BUILD_WIN32\Plex\system /E /Q /I /Y /EXCLUDE:exclude.txt  > NUL
  xcopy ..\..\media BUILD_WIN32\Plex\media /E /Q /I /Y /EXCLUDE:exclude.txt  > NUL
  xcopy ..\..\sounds BUILD_WIN32\Plex\sounds /E /Q /I /Y /EXCLUDE:exclude.txt  > NUL
    
  IF EXIST error.log del error.log > NUL
  SET build_path=%CD%
  ECHO ------------------------------------------------------------
  ECHO Building MediaStream Skin...
  cd ..\..\addons\skin.mediastream
  call build.bat > NUL
  cd %build_path%
  rem restore color and title, some scripts mess these up
  COLOR 1B
  TITLE Plex for Windows Build Script

  IF EXIST exclude.txt del exclude.txt  > NUL
  ECHO ------------------------------------------------------------
  ECHO Build Succeeded!
  GOTO NSIS_EXE

:NSIS_EXE
  ECHO ------------------------------------------------------------
  ECHO Generating installer includes...
  call genNsisIncludes.bat
  ECHO ------------------------------------------------------------
  CALL extract_git_rev.bat
  SET PLEX_SETUPFILE=Plex-Media-Center-v%GIT_REV%-en-US.exe
  ECHO Creating installer %PLEX_SETUPFILE%...
  IF EXIST %PLEX_SETUPFILE% del %PLEX_SETUPFILE% > NUL
  rem get path to makensis.exe from registry, first try tab delim
  FOR /F "tokens=2* delims=	" %%A IN ('REG QUERY "HKLM\Software\NSIS" /ve') DO SET NSISExePath=%%B

  IF NOT EXIST "%NSISExePath%" (
    rem try with space delim instead of tab
    FOR /F "tokens=2* delims= " %%A IN ('REG QUERY "HKLM\Software\NSIS" /ve') DO SET NSISExePath=%%B
  )
      
  IF NOT EXIST "%NSISExePath%" (
    rem fails on localized windows (Default) becomes (Par D�faut)
    FOR /F "tokens=3* delims=	" %%A IN ('REG QUERY "HKLM\Software\NSIS" /ve') DO SET NSISExePath=%%B
  )

  IF NOT EXIST "%NSISExePath%" (
    FOR /F "tokens=3* delims= " %%A IN ('REG QUERY "HKLM\Software\NSIS" /ve') DO SET NSISExePath=%%B
  )
  
  rem proper x64 registry checks
  IF NOT EXIST "%NSISExePath%" (
    ECHO using x64 registry entries
    FOR /F "tokens=2* delims=	" %%A IN ('REG QUERY "HKLM\Software\Wow6432Node\NSIS" /ve') DO SET NSISExePath=%%B
  )
  IF NOT EXIST "%NSISExePath%" (
    rem try with space delim instead of tab
    FOR /F "tokens=2* delims= " %%A IN ('REG QUERY "HKLM\Software\Wow6432Node\NSIS" /ve') DO SET NSISExePath=%%B
  )
  IF NOT EXIST "%NSISExePath%" (
    rem on win 7 x64, the previous fails
    FOR /F "tokens=3* delims=	" %%A IN ('REG QUERY "HKLM\Software\Wow6432Node\NSIS" /ve') DO SET NSISExePath=%%B
  )
  IF NOT EXIST "%NSISExePath%" (
    rem try with space delim instead of tab
    FOR /F "tokens=3* delims= " %%A IN ('REG QUERY "HKLM\Software\Wow6432Node\NSIS" /ve') DO SET NSISExePath=%%B
  )

  SET NSISExe=%NSISExePath%\makensis.exe
  "%NSISExe%" /V1 /X"SetCompressor /FINAL lzma" /Dplex_root="%CD%\BUILD_WIN32" /Dplex_revision="%GIT_REV%" /Dplex_target="%target%" "Plex for Windows.nsi"
  IF NOT EXIST "%PLEX_SETUPFILE%" (
	  set DIETEXT=Failed to create %PLEX_SETUPFILE%.
	  goto DIE
  )
  ECHO ------------------------------------------------------------
  ECHO Done!
  ECHO Setup is located at %CD%\%PLEX_SETUPFILE%
  ECHO ------------------------------------------------------------
  GOTO VIEWLOG_EXE
  
:DIE
  ECHO ------------------------------------------------------------
  ECHO !-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-
  ECHO    ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR
  ECHO !-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-!-
  set DIETEXT=ERROR: %DIETEXT%
  echo %DIETEXT%
  ECHO ------------------------------------------------------------

:VIEWLOG_EXE
  IF %promptlevel%==noprompt (
	goto END
  )
  SET log="%CD%\..\vs2010express\Plex\%buildconfig%\objs\BuildLog.htm"
  )
  IF NOT EXIST %log% goto END
  
  copy %log% ./buildlog.html > NUL
  
  set /P PLEX_BUILD_ANSWER=View the build log in your HTML browser? [y/n]
  if /I %PLEX_BUILD_ANSWER% NEQ y goto END
 
  SET log="%CD%\..\vs2010express\Plex\%buildconfig%\objs\" BuildLog.htm
  
  start /D%log%
  goto END

:END
  IF %promptlevel% NEQ noprompt (
  ECHO Press any key to exit...
  pause > NUL
