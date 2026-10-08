using System;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Threading;

namespace SuperRocket64 {
    internal sealed class SourceFixture {
        internal string Rom,Game,InvalidRom;
        internal SourceValidator Validator;
        internal static SourceFixture Create(string root){
            Directory.CreateDirectory(root);var bytes=new byte[8*1024*1024];bytes[0]=0x80;bytes[1]=0x37;bytes[2]=0x12;bytes[3]=0x40;
            bytes[100]=42;byte[] body={1,3,5,7},wheel={2,4,6,8};var fixture=new SourceFixture{Rom=Path.Combine(root,"synthetic-us.z64"),InvalidRom=Path.Combine(root,"invalid.z64"),Game=Path.Combine(root,"synthetic-RocketLeague")};
            File.WriteAllBytes(fixture.Rom,bytes);File.WriteAllText(fixture.InvalidRom,"not a ROM");string cooked=Path.Combine(fixture.Game,"TAGame","CookedPCConsole");Directory.CreateDirectory(cooked);File.WriteAllBytes(Path.Combine(cooked,"Body_Octane_SF.upk"),body);File.WriteAllBytes(Path.Combine(cooked,"wheel_sport80_SF.upk"),wheel);
            using(var sha=SHA1.Create())using(var sha256=SHA256.Create())fixture.Validator=new SourceValidator(Hex(sha.ComputeHash(bytes)),Hex(sha256.ComputeHash(body)),Hex(sha256.ComputeHash(wheel)));return fixture;
        }
        static string Hex(byte[] hash){return BitConverter.ToString(hash).Replace("-","").ToLowerInvariant();}
    }
    internal static class SourceValidationTests {
        internal static int Run(string root){
            var fixture=SourceFixture.Create(root);int checks=0;Action<bool,string> need=delegate(bool ok,string text){checks++;if(!ok)throw new Exception(text);};
            var token=CancellationToken.None;var result=SourceValidator.Supported.Check(fixture.Rom,fixture.Game,token);need(!result.RomValid&&!result.GameValid,"Production fingerprints accepted synthetic content");
            result=fixture.Validator.Check(fixture.Rom,fixture.Game,token);need(result.Valid,"Complete synthetic source pair failed");
            foreach(int unit in new[]{2,4}){var bytes=File.ReadAllBytes(fixture.Rom);for(int i=0;i<bytes.Length;i+=unit)Array.Reverse(bytes,i,unit);string swapped=Path.Combine(root,unit==2?"test.v64":"test.n64");File.WriteAllBytes(swapped,bytes);need(fixture.Validator.Check(swapped,fixture.Game,token).Valid,"Byte-swapped ROM rejected");}
            string zip=Path.Combine(root,"single.zip");using(var file=File.Create(zip))using(var archive=new ZipArchive(file,ZipArchiveMode.Create)){using(var stream=archive.CreateEntry("game.z64").Open()){var bytes=File.ReadAllBytes(fixture.Rom);stream.Write(bytes,0,bytes.Length);}}
            need(fixture.Validator.Check(zip,fixture.Game,token).Valid,"Single-ROM ZIP rejected");
            using(var archive=ZipFile.Open(zip,ZipArchiveMode.Update)){using(var stream=archive.CreateEntry("other.n64").Open())stream.WriteByte(0);}
            need(!fixture.Validator.Check(zip,fixture.Game,token).RomValid,"Multiple-ROM archive accepted");
            need(!fixture.Validator.Check("",fixture.Game,token).Valid,"Blank ROM accepted");need(!fixture.Validator.Check(fixture.Rom,"",token).Valid,"Blank folder accepted");need(!fixture.Validator.Check(fixture.InvalidRom,fixture.Game,token).RomValid,"Invalid file accepted");need(!fixture.Validator.Check(fixture.Rom,root,token).GameValid,"Wrong folder accepted");
            string wheel=Path.Combine(fixture.Game,"TAGame","CookedPCConsole","wheel_sport80_SF.upk");File.WriteAllBytes(wheel,new byte[]{0});need(!fixture.Validator.Check(fixture.Rom,fixture.Game,token).GameValid,"Changed package accepted");File.Delete(wheel);need(!fixture.Validator.Check(fixture.Rom,fixture.Game,token).GameValid,"Missing package accepted");
            using(var cancel=new CancellationTokenSource()){cancel.Cancel();bool stopped=false;try{fixture.Validator.Check(fixture.Rom,fixture.Game,cancel.Token);}catch(OperationCanceledException){stopped=true;}need(stopped,"Canceled validation kept running");}
            Console.WriteLine("PASS "+checks+" read-only source checks; synthetic fingerprints only in test instance");return checks;
        }
    }
}
