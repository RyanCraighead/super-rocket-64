// Windowless audit of production update/download/install state transitions.
// Synthetic state and tiny fixture payloads only; never touches an installation.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Threading;

namespace SuperRocket64 {
    internal static class PersistenceTests {
        private static string root;
        private static int checks, phases;
        private static readonly Dictionary<string,string> hashes = new Dictionary<string,string>();
        private static readonly Dictionary<string,long> times = new Dictionary<string,long>();
        private static void Need(bool ok,string label) { checks++; if(!ok)throw new Exception(label); }
        private static void Seed(string name,string value) {
            string path=Path.Combine(root,name.Replace('/',Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,value,new UTF8Encoding(false));
            hashes[path]=Guard.HashFile(path);times[path]=File.GetLastWriteTimeUtc(path).Ticks;
        }
        private static void Verify(string phase) {
            foreach(var item in hashes) {
                Need(File.Exists(item.Key)&&Guard.HashFile(item.Key)==item.Value,phase+": bytes changed: "+item.Key);
                Need(File.GetLastWriteTimeUtc(item.Key).Ticks==times[item.Key],phase+": file was rewritten: "+item.Key);
            }
            phases++;Console.WriteLine("PASS "+phase+": "+hashes.Count+" private files retain exact bytes and timestamps");
        }
        private static void Rejected(Action action) {
            bool rejected=false;try{action();}catch(IOException){rejected=true;}catch(InvalidDataException){rejected=true;}
            Need(rejected,"Expected rejected update");
        }
        private static byte[] Payload(string version) {
            var files=new Dictionary<string,byte[]>();
            foreach(string name in new[]{"sm64coopdx.exe","python/python.exe","codex/windows/seven_launcher.py","lang/English.ini"})
                files[name]=Encoding.UTF8.GetBytes("Synthetic fixture never executed: "+name+" "+version);
            var records=new Dictionary<string,object>();
            foreach(var file in files)records[file.Key]=new{size=file.Value.Length,sha256=FakeUpdateTransport.Hash(file.Value)};
            files["PACKAGE-MANIFEST.json"]=UpdateJson.Bytes(new{files=records});
            using(var output=new MemoryStream()) {
                using(var zip=new ZipArchive(output,ZipArchiveMode.Create,true))
                    foreach(var file in files)using(var entry=zip.CreateEntry(file.Key).Open())entry.Write(file.Value,0,file.Value.Length);
                return output.ToArray();
            }
        }
        [STAThread] internal static int Main(string[] args) {
            try {
                root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);
                foreach(string profile in new[]{"windows-octane","windows-mario","combined","online-host-octane","online-join-octane","online-host-mario","online-join-mario"}) {
                    string save="data/.runtime/"+profile+"/save/";
                    Seed(save+"sm64_save_file.bin","Synthetic four-slot save bytes "+profile+"\0\u0001\u00ff");
                    Seed(save+"sm64config.txt","rocket_surface_mode 2\nrocket_speed_percent 125\ncoop_host_save_slot 2\nsave-name: 2 Streaming\nsfx_volume 38\n");
                    Seed(save+"sm64config-backup.txt","Previous settings "+profile);
                }
                Seed("data/.runtime/controls/controls.cfg","# User remapped throttle, boost and camera\nrocket-bindings: 2 14 13 4 6 5 5 0 0 6 1 1 1\nkey_b 0030 FFFF FFFF\n");
                Seed("data/.runtime/controls/controls.cfg.backup","Previous remaps");
                Seed("data/.runtime/controls/migration.json","{\"schema\":1,\"source\":\"user\"}");
                Seed("data/.runtime/windows-octane/octane-model/materials/manifest.json","Synthetic optional material identity");
                Seed("data/.runtime/windows-octane/octane-model/body.bin","Synthetic model cache identity");
                Seed("data/.runtime/windows-octane/profile.json","Synthetic owned profile identity");
                new UpdatePreferences{Configured=true,ModeChosen=true,AutomaticChecks=true,AutomaticApply=false,DesktopShortcut=false,StartMenuShortcut=true,PausedVersion="0.4.0"}.Save(root);
                string prefs=UpdatePreferences.PathFor(root);hashes[prefs]=Guard.HashFile(prefs);times[prefs]=File.GetLastWriteTimeUtc(prefs).Ticks;
                string oldPackage=null;
                foreach(string version in new[]{"old","new","new"}) {
                    byte[] payload=Payload(version);string installed;
                    using(var stream=new MemoryStream(payload))installed=Installer.Install(stream,FakeUpdateTransport.Hash(payload),payload.Length,root,delegate{});
                    if(version=="old")oldPackage=installed;
                    else Need(Directory.Exists(oldPackage)&&oldPackage!=installed,"Old program package removed or replaced");
                    Verify("install/verify "+version+" package");
                }
                byte[] fixture=File.ReadAllBytes(args[1]);var transport=FakeUpdateTransport.New("0.3.0",fixture);
                var plan=ReleaseUpdates.Check(transport,"0.2.0",CancellationToken.None).Available;
                transport.PartialCancel=true;
                try{ReleaseUpdates.Download(plan,root,transport,CancellationToken.None);throw new Exception("Cancel ignored");}catch(OperationCanceledException){}
                Verify("canceled download");
                transport.PartialCancel=false;transport.Corrupt=true;
                Rejected(delegate{ReleaseUpdates.Download(plan,root,transport,CancellationToken.None);});Verify("corrupt download rejected");
                transport.Corrupt=false;string nextExe=ReleaseUpdates.Download(plan,root,transport,CancellationToken.None);
                UpdateStore.Probe(nextExe,root,plan,CancellationToken.None);Verify("download and actual headless compatibility probe");
                var old=UpdateStore.Capture(root,args[1],"0.2.0");var next=UpdateStore.Capture(root,nextExe,"0.3.0");
                UpdateStore.Save(root,new LauncherState{active=old});
                Rejected(delegate{UpdateStore.Activate(root,next,delegate{return true;},delegate{return true;});});
                Need(UpdateStore.Load(root).active.version=="0.2.0","Active-game guard changed version");Verify("active-game rejection");
                Rejected(delegate{UpdateStore.Activate(root,next,delegate{return false;},delegate{return false;});});
                Need(UpdateStore.Load(root).active.version=="0.2.0","Failed readiness changed active version");Verify("failed readiness rollback");
                int guardCalls=0;
                Rejected(delegate{UpdateStore.Activate(root,next,delegate{return ++guardCalls==2;},delegate{return true;});});
                Need(UpdateStore.Load(root).active.version=="0.2.0","Late game guard changed version");Verify("game-start race rollback");
                UpdateStore.Activate(root,next,delegate{return false;},delegate{return true;});
                Need(UpdateStore.Load(root).active.version=="0.3.0","New version inactive");Verify("successful activation");
                UpdateStore.Activate(root,old,delegate{return false;},delegate{return true;});
                Need(UpdateStore.Load(root).active.version=="0.2.0"&&File.Exists(UpdateStore.PathFor(root,next)),"Rollback lost either version");Verify("requested rollback");
                Console.WriteLine("PASS "+checks+" persistence checks across "+phases+" transitions; "+hashes.Count+" synthetic private files; no game, real installation, UI or network.");
                return 0;
            }catch(Exception error){Console.Error.WriteLine(error);return 1;}
        }
    }
}
