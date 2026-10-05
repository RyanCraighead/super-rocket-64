// Test-only launcher UI automation on a separate, never-activated Windows desktop.
// No SwitchDesktop, global input injection, game launch, or network configuration.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal static class LauncherDesktopTests {
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
            internal int cb; internal string reserved, desktop, title;
            internal int x,y,width,height,xChars,yChars,fill,flags; internal short show,reservedSize;
            internal IntPtr reserved2,input,output,error;
        }
        [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { internal IntPtr process,thread; internal uint processId,threadId; }
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateDesktop(string name,IntPtr device,IntPtr mode,uint flags,uint access,IntPtr security);
        [DllImport("user32.dll",SetLastError=true)] static extern bool CloseDesktop(IntPtr desktop);
        [DllImport("user32.dll",SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
        [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
        [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformation(IntPtr handle,int index,StringBuilder value,int length,out int needed);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string app,StringBuilder command,IntPtr processAttributes,IntPtr threadAttributes,bool inherit,uint flags,IntPtr environment,string cwd,ref Startup startup,out ProcessInfo info);
        [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
        [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr handle,out uint code);
        [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr handle,uint code);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
        static int checks;
        static void Need(bool condition,string message) { checks++; if(!condition) throw new InvalidOperationException(message); }
        static string DesktopName(IntPtr handle) { if(handle==IntPtr.Zero)throw new Win32Exception(); int length;StringBuilder value=new StringBuilder(512);if(!GetUserObjectInformation(handle,2,value,1024,out length))throw new Win32Exception();return value.ToString(); }
        static string InputDesktop() { IntPtr d=OpenInputDesktop(0,false,1);try{return DesktopName(d);}finally{if(d!=IntPtr.Zero)CloseDesktop(d);} }
        static T Field<T>(LauncherForm form,string name) {return (T)typeof(LauncherForm).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).GetValue(form);}
        static void Call(LauncherForm form,string name,params object[] args) {typeof(LauncherForm).GetMethod(name,BindingFlags.Instance|BindingFlags.NonPublic).Invoke(form,args);Application.DoEvents();}
        static Button Button(Control parent,string text) {foreach(Control c in parent.Controls){Button b=c as Button;if(b!=null&&b.Text==text)return b;if(c.HasChildren){b=Button(c,text);if(b!=null)return b;}}return null;}
        static void Click(Control page,string text) {Button b=Button(page,text);Need(b!=null&&b.Visible&&b.Enabled,"Missing usable button: "+text);b.PerformClick();Application.DoEvents();}
        static void Page(LauncherForm form,string name) {Need(Field<Control>(form,name).Visible,"Wrong page: "+name);}
        static void Snapshot(LauncherForm form,string output,string name) {Application.DoEvents();form.PerformLayout();using(Bitmap b=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(b,new Rectangle(Point.Empty,b.Size));b.Save(Path.Combine(output,name+".png"),ImageFormat.Png);}}
        static void AssertSeparate(string expected) {Need(DesktopName(GetThreadDesktop(GetCurrentThreadId()))==expected,"Wrong thread desktop; refusing UI");Need(InputDesktop()!=expected,"Test desktop is active; refusing UI");}
        static int Child(string expected,string output) {
            try {
                AssertSeparate(expected);Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
                using(LauncherForm form=new LauncherForm(Path.Combine(output,"synthetic-install"))) {
                    // Disable automatic read-only address discovery so screenshots contain no private addresses.
                    Field<Button>(form,"refreshAddresses").Enabled=false;
                    Field<Label>(form,"addressStatus").Text="Test fixture: use a reachable LAN or existing Tailscale address.";
                    form.Show();Application.DoEvents();AssertSeparate(expected);Page(form,"homePage");
                    Snapshot(form,output,"home-900");
                    Click(Field<Control>(form,"homePage"),"Setup SM64 + Rocket League");Page(form,"setupPage");
                    Field<TextBox>(form,"rom").Text="C:\\Owned Games\\sm64.us.z64";
                    Click(Field<Control>(form,"setupPage"),"Back");Page(form,"homePage");
                    Click(Field<Control>(form,"homePage"),"Setup SM64 + Rocket League");
                    Need(Field<TextBox>(form,"rom").Text.EndsWith("sm64.us.z64"),"Back lost input");Snapshot(form,output,"setup-900");
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"optionalPromptPage"));
                    Click(Field<Control>(form,"optionalPromptPage"),"No, continue");Page(form,"homePage");
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"optionalPromptPage"));
                    Click(Field<Control>(form,"optionalPromptPage"),"Yes, choose a game");Page(form,"optionalSetupPage");
                    ComboBox optional=Field<ComboBox>(form,"optionalCharacter");Need(optional.Items.Count==5,"Optional game count");
                    for(int i=0;i<5;i++){Field<TextBox>(form,"optionalRom").Text="previous-game";optional.SelectedIndex=(i+1)%5;Need(Field<TextBox>(form,"optionalRom").Text=="","Game choice reused wrong ROM");Need(Field<Label>(form,"optionalFormat").Text.Length>35,"Missing accepted format");}
                    Snapshot(form,output,"optional-900");Click(Field<Control>(form,"optionalSetupPage"),"Back");Page(form,"optionalPromptPage");
                    Click(Field<Control>(form,"optionalPromptPage"),"No, continue");Click(Field<Control>(form,"homePage"),"Online");Page(form,"onlineChoicePage");
                    Click(Field<Control>(form,"onlineChoicePage"),"Host");Page(form,"onlinePage");
                    Need(Field<NumericUpDown>(form,"port").Value==7777,"Default port");Need(Field<ComboBox>(form,"onlineCharacter").Items.Count==2,"Unsupported online character advertised");
                    Need(Field<Control>(form,"hostAddressRow").Visible&&!Field<Control>(form,"joinAddressRow").Visible,"Host layout");
                    Field<NumericUpDown>(form,"port").Value=8123;Snapshot(form,output,"host-900");
                    Click(Field<Control>(form,"onlinePage"),"Back");Click(Field<Control>(form,"onlineChoicePage"),"Join");
                    Need(Field<Control>(form,"joinAddressRow").Visible&&!Field<Control>(form,"hostAddressRow").Visible,"Join layout");Need(Field<NumericUpDown>(form,"port").Value==8123,"Port edit lost");
                    Field<TextBox>(form,"host").Text="192.0.2.10";Snapshot(form,output,"join-900");
                    // Recover from a real launcher operation failure with no embedded payload.
                    Click(Field<Control>(form,"onlinePage"),"Back");Click(Field<Control>(form,"onlineChoicePage"),"Back");
                    Click(Field<Control>(form,"homePage"),"Refresh setup status");
                    Stopwatch wait=Stopwatch.StartNew();while(Field<bool>(form,"running")&&wait.ElapsedMilliseconds<5000){Application.DoEvents();Thread.Sleep(10);}
                    Need(!Field<bool>(form,"running")&&Field<Control>(form,"pageHost").Enabled,"Failed operation did not allow retry");
                    Need(Field<RichTextBox>(form,"output").Text.Contains("Stopped:"),"Missing actionable operation failure");
                    Need(!Field<Button>(form,"cancelOperation").Visible,"Cancel remained enabled after operation");
                    form.Size=form.MinimumSize;
                    foreach(string page in new string[]{"homePage","setupPage","optionalSetupPage","onlinePage"}){Call(form,"ShowPage",Field<FlowLayoutPanel>(form,page));Snapshot(form,output,page+"-minimum");}
                    AssertSeparate(expected);form.Close();
                }
                File.WriteAllText(Path.Combine(output,"ui-result.json"),new JavaScriptSerializer().Serialize(new{passed=true,checks=checks,separate_desktop=expected,input_desktop=InputDesktop(),game_started=false,helper_started=
                    false
                ,address_detection_used=false}));return 0;
            } catch(Exception e){File.WriteAllText(Path.Combine(output,"ui-error.txt"),e.ToString());return 1;}
        }
        [STAThread] static int Main(string[] args) {
            if(args.Length==3&&args[0]=="--child")return Child(args[1],args[2]);
            if(args.Length!=1)return 2;string output=Path.GetFullPath(args[0]);Directory.CreateDirectory(output);
            string input=InputDesktop(),name="SR64LauncherQA_"+Guid.NewGuid().ToString("N");IntPtr desktop=CreateDesktop(name,IntPtr.Zero,IntPtr.Zero,0,0x01ff,IntPtr.Zero);
            if(desktop==IntPtr.Zero)throw new Win32Exception();ProcessInfo pi=new ProcessInfo();
            try {
                Need(DesktopName(desktop)==name&&input!=name,"Unsafe desktop selection");
                string exe=Assembly.GetExecutingAssembly().Location;Startup si=new Startup{cb=Marshal.SizeOf(typeof(Startup)),desktop="WinSta0\\"+name,flags=0x80};
                StringBuilder command=new StringBuilder(Commands.Quote(exe)+" --child "+Commands.Quote(name)+" "+Commands.Quote(output));
                if(!CreateProcess(exe,command,IntPtr.Zero,IntPtr.Zero,false,0x08000000,IntPtr.Zero,output,ref si,out pi))throw new Win32Exception();
                Stopwatch timeout=Stopwatch.StartNew();bool childFocused=false;
                while(WaitForSingleObject(pi.process,100)==258){uint pid;GetWindowThreadProcessId(GetForegroundWindow(),out pid);childFocused|=pid==pi.processId;Need(InputDesktop()!=name,"Input desktop unexpectedly changed");if(timeout.ElapsedMilliseconds>
                    30000
                ){TerminateProcess(pi.process,99);throw new TimeoutException("Only the test child was terminated");}}
                uint result;GetExitCodeProcess(pi.process,out result);Need(!childFocused&&InputDesktop()!=name,"Test took input desktop");
                File.WriteAllText(Path.Combine(output,"desktop-isolation.json"),new JavaScriptSerializer().Serialize(new{child_exit=result,child_foreground_seen=childFocused,input_desktop_before=input,input_desktop_after=InputDesktop(),test_desktop=name,never_called_switch_desktop=true}));
                Console.WriteLine("Separate-desktop launcher UI test exit="+result+"; no input-desktop switch; child foreground="+childFocused);return (int)result;
            } finally {if(pi.thread!=IntPtr.Zero)CloseHandle(pi.thread);if(pi.process!=IntPtr.Zero)CloseHandle(pi.process);CloseDesktop(desktop);}
        }
    }
}
