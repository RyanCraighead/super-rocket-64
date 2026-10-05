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
        static void WaitUpdate(LauncherForm form) {
            Stopwatch timer=Stopwatch.StartNew();while(Field<bool>(form,"running")&&timer.ElapsedMilliseconds<15000){Application.DoEvents();Thread.Sleep(20);}
            Need(!Field<bool>(form,"running")&&Field<Control>(form,"pageHost").Enabled,"Update did not finish/re-enable UI");
        }
        static int Child(string expected,string output) {
            try {
                AssertSeparate(expected);Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
                using(LauncherForm form=new LauncherForm(Path.Combine(output,"synthetic-install"))) {
                    // Disable automatic read-only address discovery so screenshots contain no private addresses.
                    Field<Button>(form,"refreshAddresses").Enabled=false;
                    Field<Label>(form,"addressStatus").Text="Test fixture: use a reachable LAN or existing Tailscale address.";
                    var transport = FakeUpdateTransport.New("0.3.0", Encoding.ASCII.GetBytes("synthetic update"));
                    form.UpdateTransport = transport;
                    string desktopFolder = Path.Combine(output,"shortcut-Desktop"), programsFolder = Path.Combine(output,"shortcut-Programs");
                    Directory.CreateDirectory(desktopFolder); Directory.CreateDirectory(programsFolder);
                    form.UpdateShortcuts = delegate(string root, bool desktopChoice, bool menuChoice) { LauncherShortcuts.Apply(root,desktopChoice,menuChoice,desktopFolder,programsFolder); };
                    form.Show();Application.DoEvents();AssertSeparate(expected);Page(form,"locationPage");
                    Need(Button(form,"Play Offline")!=null,"Missing offline path");
                    foreach(Control c in form.Controls) Need(!(c is RichTextBox),"Technical output UI present");
                    Snapshot(form,output,"01-location");
                    // The fixture deliberately has no embedded payload: exercise
                    // the real failed operation and recovery UI without a game.
                    Click(Field<Control>(form,"locationPage"),"Next");WaitUpdate(form);Page(form,"failurePage");
                    Need(Field<Label>(form,"failureText").Text.Length>15,"Missing actionable failure");
                    Need(Button(Field<Control>(form,"failurePage"),"Repair program files")!=null,"No repair action");
                    Click(Field<Control>(form,"failurePage"),"Back / change source");Page(form,"setupPage");
                    Snapshot(form,output,"02-sources");
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"extrasPage"));
                    Need(Field<RadioButton>(form,"extrasNo").Checked&&!Field<Control>(form,"extraInputs").Visible,"Extras not optional by default");
                    Field<RadioButton>(form,"extrasYes").Checked=true;Application.DoEvents();
                    Need(Field<Control>(form,"extraInputs").Visible,"Yes did not reveal extras");
                    CheckBox[] choices=Field<CheckBox[]>(form,"extraChoices");TextBox[] sources=Field<TextBox[]>(form,"extraSources");
                    Need(choices.Length==5&&sources.Length==5,"Optional source count");
                    choices[0].Checked=choices[3].Checked=true;sources[0].Text="separate source A";sources[3].Text="separate source B";
                    Need(sources[0].Text!=sources[3].Text,"Optional choices share input");Snapshot(form,output,"03-extras");
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"readyPage"));
                    Need(!Field<RadioButton>(form,"readyAuto").Checked&&!Field<RadioButton>(form,"readyManual").Checked,"Update install consent inferred");
                    Click(Field<Control>(form,"readyPage"),"Open launcher");Page(form,"readyPage");
                    Need(Field<Label>(form,"notice").Text.Contains("Choose"),"No update choice validation");
                    Snapshot(form,output,"05-ready");
                    Call(form,"ShowUpdateSettings");
                    Field<RadioButton>(form,"manualUpdates").Checked=true;Click(Field<Control>(form,"settingsPage"),"Save preferences");
                    string installRoot=Field<TextBox>(form,"install").Text;
                    var prefs=UpdatePreferences.Load(installRoot);Need(prefs.ModeChosen&&!prefs.AutomaticChecks&&!prefs.AutomaticApply,"Manual mode persistence");
                    Call(form,"ShowUpdateSettings");Click(Field<Control>(form,"settingsPage"),"Check for updates");WaitUpdate(form);Page(form,"updatePage");
                    Need(Field<Label>(form,"updateMessage").Text.Contains("0.3.0"),"Available release missing");
                    Need(!Field<Button>(form,"useInstalled").Visible,"Incomplete install offered Play");
                    transport.Offline=true;Click(Field<Control>(form,"updatePage"),"Retry update check");WaitUpdate(form);Page(form,"updatePage");
                    Need(Field<Label>(form,"updateMessage").Text.Contains("retained"),"Offline fallback missing");transport.Offline=false;
                    Call(form,"ShowUpdateSettings");Field<RadioButton>(form,"automaticUpdates").Checked=true;Click(Field<Control>(form,"settingsPage"),"Save preferences");
                    prefs=UpdatePreferences.Load(installRoot);Need(prefs.ModeChosen&&prefs.AutomaticApply&&prefs.AutomaticChecks,"Automatic mode not persisted");
                    transport.Corrupt=true;form.UpdateGameActive=delegate{return false;};
                    Call(form,"CheckUpdates",true);WaitUpdate(form);Page(form,"updatePage");
                    Need(transport.Downloads==1&&UpdatePreferences.Load(installRoot).PausedVersion=="0.3.0","Automatic mode did not download/verify/pause corrupt release");transport.Corrupt=false;
                    prefs.PausedVersion="0.3.0";prefs.Save(installRoot);Call(form,"CheckUpdates",true);WaitUpdate(form);Page(form,"updatePage");
                    Need(Field<Label>(form,"updateMessage").Text.Contains("paused"),"Failed version retry loop");
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Click(Field<Control>(form,"homePage"),"Online");
                    Click(Field<Control>(form,"onlineChoicePage"),"Host");Page(form,"onlinePage");
                    Need(Field<NumericUpDown>(form,"port").Value==7777&&Field<ComboBox>(form,"onlineCharacter").Items.Count==2,"Online defaults/support");Snapshot(form,output,"host");
                    Click(Field<Control>(form,"onlinePage"),"Back");Click(Field<Control>(form,"onlineChoicePage"),"Join");
                    Need(Field<Control>(form,"joinAddressRow").Visible&&!Field<Control>(form,"hostAddressRow").Visible,"Join layout");Snapshot(form,output,"join");
                    string endpoint="[fd7a:115c:a1e0::1]:8123";int selectedPort=7777;LauncherForm.ParseEndpoint(ref endpoint,ref selectedPort);Need(endpoint=="fd7a:115c:a1e0::1"&&selectedPort==8123,"IPv6 endpoint parse");
                    endpoint="192.0.2.1:9123";LauncherForm.ParseEndpoint(ref endpoint,ref selectedPort);Need(endpoint=="192.0.2.1"&&selectedPort==9123,"IPv4 endpoint parse");
                    Need(LauncherForm.FriendlyStage("private raw converter diagnostics")==null,"Raw output reached UI");
                    form.Size=form.MinimumSize;
                    foreach(string page in new string[]{"locationPage","setupPage","extrasPage","readyPage","onlinePage"}){Call(form,"ShowPage",Field<FlowLayoutPanel>(form,page));Snapshot(form,output,page+"-minimum");}
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
