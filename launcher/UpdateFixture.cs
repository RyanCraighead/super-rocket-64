// Test-only child process. Never opens a window, starts a game, or uses a network.
using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
namespace SuperRocket64 {
    internal static class UpdateFixture {
        private static int Main(string[] args) {
            string executable = Assembly.GetExecutingAssembly().Location, root = null, token = null;
            for (int i=0;i<args.Length;i++) { if(args[i]=="--install-dir")root=args[++i];else if(args[i]=="--update-ready")token=args[++i]; }
            string modePath=Path.Combine(Path.GetDirectoryName(executable),"fixture-mode.txt"); string mode=File.Exists(modePath)?File.ReadAllText(modePath):"ok";
            if (mode=="exit")return 3;
            if (mode=="sleep")Thread.Sleep(120000);
            if (Array.IndexOf(args,"--update-probe")>=0) {
                if(mode=="flood"){for(int i=0;i<1000;i++)Console.WriteLine(new string('x',2048));return 0;}
                Console.WriteLine(new JavaScriptSerializer().Serialize(new {verified=true,version=mode=="wrong-version"?"8.0.0":"0.3.0",update_protocol=1,data_contract=1,payload_sha256=new string('a',64)}));return 0;
            }
            if(token==null||root==null)return 4;
            string hash;using(var sha=SHA256.Create())using(var stream=File.OpenRead(executable))hash=BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
            string ready=Path.Combine(root,"launcher-ready-"+token+".json");Directory.CreateDirectory(root);
            File.WriteAllText(ready,new JavaScriptSerializer().Serialize(new {sha256=mode=="wrong-hash"?new string('b',64):hash}),new UTF8Encoding(false));
            Stopwatch timer=Stopwatch.StartNew();
            while(timer.ElapsedMilliseconds<35000){if(File.Exists(ready+".commit")){File.Delete(ready+".commit");File.WriteAllText(Path.Combine(root,"fixture-restarted.txt"),hash);return 0;}Thread.Sleep(25);}
            return 5;
        }
    }
}
