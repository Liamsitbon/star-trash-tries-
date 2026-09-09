import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
native=subprocess.check_output([str(root/'build/protocol_test'),'--golden'],text=True).strip()
project=root/'tests/dotnet/GoldenWire.csproj'
subprocess.run(['dotnet','build',str(project),'--nologo','--verbosity','quiet'],check=True)
managed=subprocess.check_output(['dotnet',str(root/'tests/dotnet/bin/Debug/net10.0/GoldenWire.dll')],text=True).strip()
assert native==managed, 'Native protocol bytes differ from .NET BinaryWriter oracle'
print('All 9 client opcodes + long UTF8 length fixture match .NET bytes exactly')
