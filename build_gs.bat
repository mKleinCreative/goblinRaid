@echo off
set UBT="D:\Epic Games\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
%UBT% MyProjectEditor Win64 Development -Project="D:\goblinRaid\GoblinSiege 5.8\MyProject.uproject" -NoMutex -Executor=ParallelExecutor -NoUba -NoUbaLocal > "D:\goblinRaid\ubt18.log" 2>&1
echo BUILD_EXITCODE=%ERRORLEVEL% >> "D:\goblinRaid\ubt18.log"
