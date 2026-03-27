## compile deterministic_sim.cpp to .exe and .dll

(ON ROOT OF THE PROJECT)
> mkdir build
> cd build
> cmake ..
> cmake --build . --config Release

## compile MonoUI to .exe

(ON TOOLS/CSHARP/MONOUI)
(POWERSHELL)
> Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
> ./setup-dotnet.ps1
> dotnet build --configuration Debug
> dotnet run --configuration Debug