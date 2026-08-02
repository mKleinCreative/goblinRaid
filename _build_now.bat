@echo off
REM -NoMutex: a stale global UnrealBuildTool mutex has been surviving with no process
REM holding it (no cl.exe, no dotnet, no cmd), and Build.bat waits on it by default -
REM which looks exactly like a slow build and never ends. -NoMutex skips the lock.
REM Safe here because we verify no compiler is running before launching.
set LOG=D:\goblinRaid\_build_now.log
echo Building MyProjectEditor... > "%LOG%"
call "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" MyProjectEditor Win64 Development -Project="D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject" -NoMutex >> "%LOG%" 2>&1
echo EXITCODE=%ERRORLEVEL% >> "%LOG%"
