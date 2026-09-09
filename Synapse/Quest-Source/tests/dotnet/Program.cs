using System;
using System.IO;
using System.Text;
// Independent BinaryWriter oracle. Synthetic credentials only; no network.
static class Program {
    static void Packet(string name, byte opcode, Action<BinaryWriter> payload) {
        using var stream=new MemoryStream(); using var writer=new BinaryWriter(stream,Encoding.UTF8,true);
        writer.Write((ushort)0);writer.Write(opcode);payload(writer);writer.Flush();
        ushort length=checked((ushort)(stream.Length-2));stream.Position=0;writer.Write(length);writer.Flush();
        Console.WriteLine(name+" "+Convert.ToHexString(stream.ToArray()).ToLowerInvariant());
    }
    static void Main() {
        Packet("auth",0,w=>{ w.Write("123456");w.Write("שלום 🦋");w.Write((byte)1);w.Write("synthetic-test-token");w.Write("1.40.8_7379");w.Write("test-listing"); });
        Packet("disconnect",1,w=>w.Write((byte)2));Packet("ping",2,w=>w.Write(1.25f));
        Packet("chatter",3,w=>w.Write(true));Packet("division",4,w=>w.Write(-1));
        Packet("chat",5,w=>w.Write("שלום"));Packet("command",6,w=>w.Write("/test synthetic"));
        Packet("score",7,w=>w.Write("{\"division\":0,\"index\":1,\"score\":123,\"percentage\":98.5}"));
        Packet("leaderboard",8,w=>{w.Write(-1);w.Write(2);w.Write(true);});
        Packet("long-string",5,w=>w.Write(new string('x',300)));
    }
}
