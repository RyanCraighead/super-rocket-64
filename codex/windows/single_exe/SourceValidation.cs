using System;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed class SourceSelectionException : Exception { internal SourceSelectionException(string message):base(message){} }
    internal sealed class SourceValidationResult {
        internal bool RomValid, GameValid;
        internal string RomMessage, GameMessage;
        internal bool Valid { get { return RomValid && GameValid; } }
    }
    // Read-only counterparts of seven_launcher.sm64_bytes and export_octane.check_game.
    // Keep these fingerprints aligned with the packaged extraction validators.
    internal sealed class SourceValidator {
        internal const string Sm64Sha1 = "9bef1128717f958171a4afac3ed78ee2bb4e86ce";
        internal const string BodySha256 = "bedf7fc0d64ab2c6d4f2620a946bb88e600f779c3e924fb80ab96c431d67f7e6";
        internal const string WheelSha256 = "9b2582f69e6bf2fd06272b9b545dfd33cfc931f31d373b63a1078a747d902560";
        internal static readonly SourceValidator Supported = new SourceValidator(Sm64Sha1, BodySha256, WheelSha256);
        private readonly string romHash, bodyHash, wheelHash;
        // Tests inject synthetic fingerprints into a separate validator instance;
        // the production form always starts with Supported.Check.
        internal SourceValidator(string rom, string body, string wheel) { romHash=rom; bodyHash=body; wheelHash=wheel; }
        internal SourceValidationResult Check(string rom, string game, CancellationToken token) {
            var result = new SourceValidationResult();
            try { ValidateRom(rom, token); result.RomValid=true; result.RomMessage="SM64 US ROM selected."; }
            catch(OperationCanceledException){throw;} catch(Exception e){result.RomMessage=Feedback(e,"Could not read this ROM. Choose another file.");}
            token.ThrowIfCancellationRequested();
            try { ValidateGame(game, token); result.GameValid=true; result.GameMessage="Supported Rocket League folder selected."; }
            catch(OperationCanceledException){throw;} catch(Exception e){result.GameMessage=Feedback(e,"Could not read this folder. Check access and try again.");}
            return result;
        }
        private static string Feedback(Exception error,string fallback){return error is SourceSelectionException?error.Message:fallback;}
        private static void Need(bool condition,string message){if(!condition)throw new SourceSelectionException(message);}
        private static byte[] ReadBounded(Stream stream,int limit,CancellationToken token){
            using(var output=new MemoryStream()){
                var buffer=new byte[65536];int read;
                while((read=stream.Read(buffer,0,buffer.Length))>0){token.ThrowIfCancellationRequested();Need(output.Length+read<=limit,"ROM is too large. Choose the original 8 MB SM64 US ROM.");output.Write(buffer,0,read);}
                return output.ToArray();
            }
        }
        internal static void Normalize(byte[] bytes){
            Need(bytes.Length==8*1024*1024,"Choose the original 8 MB SM64 US ROM.");
            if(bytes[0]==0x80&&bytes[1]==0x37&&bytes[2]==0x12&&bytes[3]==0x40)return;
            int unit=bytes[0]==0x37&&bytes[1]==0x80&&bytes[2]==0x40&&bytes[3]==0x12?2:bytes[0]==0x40&&bytes[1]==0x12&&bytes[2]==0x37&&bytes[3]==0x80?4:0;
            Need(unit!=0,"This is not a supported N64 ROM. Choose SM64 US.");
            for(int i=0;i<bytes.Length;i+=unit)Array.Reverse(bytes,i,unit);
        }
        private void ValidateRom(string path,CancellationToken token){
            Need(!String.IsNullOrWhiteSpace(path)&&File.Exists(path),"Choose your SM64 US ROM.");
            byte[] bytes;
            using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)){
                Need(file.Length>0&&file.Length<=70*1024*1024,"Choose a ROM or single-ROM ZIP under 70 MB.");
                bool zip=file.ReadByte()=='P'&&file.ReadByte()=='K';file.Position=0;
                if(zip){
                    using(var archive=new ZipArchive(file,ZipArchiveMode.Read,true)){
                        Need(archive.Entries.Count<=32,"ZIP has too many files. Choose a single-ROM ZIP.");ZipArchiveEntry selected=null;
                        foreach(var entry in archive.Entries){string ext=Path.GetExtension(entry.Name).ToLowerInvariant();if(ext!=".z64"&&ext!=".v64"&&ext!=".n64")continue;Need(selected==null,"ZIP must contain exactly one N64 ROM.");selected=entry;}
                        Need(selected!=null&&selected.Length==8*1024*1024,"ZIP must contain the original 8 MB SM64 US ROM.");
                        using(var input=selected.Open())bytes=ReadBounded(input,8*1024*1024,token);
                    }
                }else bytes=ReadBounded(file,8*1024*1024,token);
            }
            token.ThrowIfCancellationRequested();Normalize(bytes);
            using(var hash=SHA1.Create())Need(BitConverter.ToString(hash.ComputeHash(bytes)).Replace("-","").ToLowerInvariant()==romHash,"This ROM does not match SM64 US. Choose the original US version.");
        }
        private static string HashPackage(string path,CancellationToken token){
            Need(File.Exists(path),"Choose the Rocket League folder containing TAGame.");
            using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read))using(var hash=SHA256.Create()){
                Need(file.Length>0&&file.Length<=64*1024*1024,"Rocket League files are invalid. Verify the game installation.");
                var buffer=new byte[65536];long total=0;int read;
                while((read=file.Read(buffer,0,buffer.Length))>0){token.ThrowIfCancellationRequested();total+=read;Need(total<=64*1024*1024,"Rocket League files changed. Wait for its update to finish.");hash.TransformBlock(buffer,0,read,buffer,0);}
                Need(total==file.Length,"Rocket League files changed. Wait for its update to finish.");hash.TransformFinalBlock(new byte[0],0,0);return BitConverter.ToString(hash.Hash).Replace("-","").ToLowerInvariant();
            }
        }
        private void ValidateGame(string path,CancellationToken token){
            Need(!String.IsNullOrWhiteSpace(path)&&Directory.Exists(path),"Choose your Rocket League folder.");
            string folder=Path.Combine(path,"TAGame","CookedPCConsole");
            string body=HashPackage(Path.Combine(folder,"Body_Octane_SF.upk"),token),wheel=HashPackage(Path.Combine(folder,"wheel_sport80_SF.upk"),token);
            Need(body==bodyHash&&wheel==wheelHash,"This Rocket League version is not supported yet. Choose a supported installation.");
        }
    }
    internal sealed partial class LauncherForm {
        private readonly Label romFeedback=TextBlock("Choose your SM64 US ROM."),gameFeedback=TextBlock("Choose your Rocket League folder.");
        private readonly System.Windows.Forms.Timer sourceDelay=new System.Windows.Forms.Timer{Interval=300};
        private CancellationTokenSource sourceCancellation;
        private int sourceGeneration;
        private bool sourceValidationRunning;
        private bool sourceNextRequested;
        private string checkedRom,checkedGame;
        private Button sourceNext;
        internal Func<string,string,CancellationToken,SourceValidationResult> CheckSources=SourceValidator.Supported.Check;
        private void InitializeSourceValidation(){
            rom.TextChanged+=delegate{QueueSourceValidation();};game.TextChanged+=delegate{QueueSourceValidation();};
            sourceDelay.Tick+=delegate{sourceDelay.Stop();ValidateSelectedSources(false);};
            FormClosed+=delegate{sourceDelay.Stop();sourceDelay.Dispose();if(sourceCancellation!=null)sourceCancellation.Cancel();};
            sourceNext.Enabled=false;
        }
        private void QueueSourceValidation(){
            sourceGeneration++;sourceNext.Enabled=false;sourceNextRequested=false;checkedRom=checkedGame=null;
            if(sourceCancellation!=null){sourceCancellation.Cancel();sourceCancellation=null;}
            sourceValidationRunning=false;romFeedback.Text=String.IsNullOrWhiteSpace(rom.Text)?"Choose your SM64 US ROM.":"Checking ROM...";gameFeedback.Text=String.IsNullOrWhiteSpace(game.Text)?"Choose your Rocket League folder.":"Checking folder...";
            sourceDelay.Stop();if(setupPage.Visible)sourceDelay.Start();
        }
        private void LeaveSourceValidation(){sourceDelay.Stop();sourceGeneration++;sourceNextRequested=false;sourceValidationRunning=false;if(sourceCancellation!=null){sourceCancellation.Cancel();sourceCancellation=null;}}
        private void ValidateSelectedSources(bool advance){
            sourceDelay.Stop();if(sourceCancellation!=null)sourceCancellation.Cancel();
            var cancellation=new CancellationTokenSource();CancellationToken token=cancellation.Token;sourceCancellation=cancellation;int generation=++sourceGeneration;
            string selectedRom=rom.Text.Trim(),selectedGame=game.Text.Trim();sourceNext.Enabled=false;sourceValidationRunning=true;sourceNextRequested=advance;
            romFeedback.Text=String.IsNullOrEmpty(selectedRom)?"Choose your SM64 US ROM.":"Checking ROM...";gameFeedback.Text=String.IsNullOrEmpty(selectedGame)?"Choose your Rocket League folder.":"Checking folder...";
            Task.Factory.StartNew(delegate{
                SourceValidationResult result=null;try{result=CheckSources(selectedRom,selectedGame,token);}catch(OperationCanceledException){}catch(Exception){result=new SourceValidationResult{RomMessage="Could not check these sources. Try selecting them again.",GameMessage="Check folder access and try again."};}
                if(!IsDisposed&&!Disposing)try{BeginInvoke(new Action(delegate{
                    try {
                    if(generation!=sourceGeneration||token.IsCancellationRequested)return;
                    sourceCancellation=null;
                    sourceValidationRunning=false;if(result==null)return;
                    romFeedback.Text=result.RomMessage;gameFeedback.Text=result.GameMessage;sourceNext.Enabled=result.Valid;
                    checkedRom=result.Valid?selectedRom:null;checkedGame=result.Valid?selectedGame:null;
                    if(sourceNextRequested&&result.Valid){sourceNextRequested=false;var args=Commands.Setup("octane",checkedRom,"","",checkedGame,true);args[0]="preflight";BeginOperation(args,false,delegate{ShowPage(extrasPage);});}
                    } catch(Exception error) { sourceNextRequested=false;sourceNext.Enabled=false;notice.Text=PlainFailure(error.Message); }
                    finally { cancellation.Dispose(); }
                }));}catch(InvalidOperationException){cancellation.Dispose();}else cancellation.Dispose();
            });
        }
        private void ContinueFromSources(){
            Guard.Need(!sourceValidationRunning&&sourceNext.Enabled&&checkedRom==rom.Text.Trim()&&checkedGame==game.Text.Trim(),"Choose a valid SM64 US ROM and Rocket League folder.");
            // Recheck the files on Next; a changed file cannot use a stale result.
            ValidateSelectedSources(true);
        }
    }
}
