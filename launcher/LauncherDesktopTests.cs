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
        static void Set(LauncherForm form,string name,object value) {typeof(LauncherForm).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).SetValue(form,value);}
        [StructLayout(LayoutKind.Sequential)] struct DpiRect {internal int Left,Top,Right,Bottom;}
        static void Dpi(LauncherForm form,int dpi) {
            var r=new DpiRect{Left=0,Top=0,Right=1152*dpi/96,Bottom=768*dpi/96};IntPtr ptr=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(DpiRect)));
            try{Marshal.StructureToPtr(r,ptr,false);Message m=Message.Create(form.Handle,0x02E0,new IntPtr(dpi|(dpi<<16)),ptr);typeof(LauncherForm).GetMethod("WndProc",BindingFlags.NonPublic|BindingFlags.Instance).Invoke(form,new object[]{m});Application.DoEvents();Rectangle work=Screen.FromRectangle(new Rectangle(0,0,r.Right,r.Bottom)).WorkingArea;Need(form.ClientSize==new Size(Math.Min(r.Right,work.Width),Math.Min(r.Bottom,work.Height)),"DPI suggested bounds not applied within working area: "+dpi+" actual="+form.ClientSize);}finally{Marshal.FreeHGlobal(ptr);}
        }
        static void Escape(LauncherForm form){typeof(Form).GetMethod("ProcessDialogKey",BindingFlags.NonPublic|BindingFlags.Instance).Invoke(form,new object[]{Keys.Escape});Application.DoEvents();}
        static void Reach(LauncherForm form,Control control){Need(control!=null&&control.Visible&&control.Enabled&&control.CanSelect,"Action cannot receive keyboard focus");Need(control.Focus(),"Keyboard focus failed");Application.DoEvents();Panel host=Field<Panel>(form,"pageHost");host.ScrollControlIntoView(control);Application.DoEvents();Rectangle bounds=host.RectangleToClient(control.RectangleToScreen(control.ClientRectangle));Need(bounds.Left>=0&&bounds.Right<=host.ClientSize.Width+2&&bounds.Top>=0&&bounds.Bottom<=host.ClientSize.Height+2,"Scrolled action is clipped: "+control.Text);for(Control parent=control.Parent;parent!=null&&parent!=host;parent=parent.Parent){bounds=parent.RectangleToClient(control.RectangleToScreen(control.ClientRectangle));Need(bounds.Left>=0&&bounds.Right<=parent.ClientSize.Width+2,"Action clipped by its parent: "+control.Text);}}
        static void Call(LauncherForm form,string name,params object[] args) {typeof(LauncherForm).GetMethod(name,BindingFlags.Instance|BindingFlags.NonPublic).Invoke(form,args);Application.DoEvents();}
        static Button Button(Control parent,string text) {foreach(Control c in parent.Controls){Button b=c as Button;if(b!=null&&b.Text==text)return b;if(c.HasChildren){b=Button(c,text);if(b!=null)return b;}}return null;}
        static void Click(Control page,string text) {Button b=Button(page,text);Need(b!=null&&b.Visible&&b.Enabled,"Missing usable button: "+text);b.PerformClick();Application.DoEvents();}
        static void Page(LauncherForm form,string name) {Need(Field<Control>(form,name).Visible,"Wrong page: "+name+"; notice="+Field<Label>(form,"notice").Text);}
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
                    // Seed only this synthetic root so real running games need not be stopped.
                    string fixtureRoot=Field<TextBox>(form,"install").Text;
                    UpdateStore.Save(fixtureRoot,new LauncherState{active=UpdateStore.Capture(fixtureRoot,UpdateStore.CurrentExe,UpdateBuild.Version)});
                    string desktopFolder = Path.Combine(output,"shortcut-Desktop"), programsFolder = Path.Combine(output,"shortcut-Programs");
                    Directory.CreateDirectory(desktopFolder); Directory.CreateDirectory(programsFolder);
                    form.UpdateShortcuts = delegate(string root, bool desktopChoice, bool menuChoice) { LauncherShortcuts.Apply(root,desktopChoice,menuChoice,desktopFolder,programsFolder); };
                    form.Show();Application.DoEvents();AssertSeparate(expected);Page(form,"locationPage");
                    Need(Button(form,"Play Offline")!=null,"Missing offline path");
                    foreach(Control c in form.Controls) Need(!(c is RichTextBox),"Technical output UI present");
                    form.ClientSize=new Size(1536,1024);Snapshot(form,output,"01-location");
                    // The fixture deliberately has no embedded payload: exercise
                    // the real failed operation and recovery UI without a game.
                    Click(Field<Control>(form,"locationPage"),"Next");WaitUpdate(form);Page(form,"failurePage");
                    Need(Field<Label>(form,"failureText").Text.Length>15,"Missing actionable failure");
                    Need(Button(Field<Control>(form,"failurePage"),"Repair program files")!=null,"No repair action");
                    Snapshot(form,output,"recovery");
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
                    Snapshot(form,output,"settings-first-run");
                    Field<RadioButton>(form,"manualUpdates").Checked=true;Click(Field<Control>(form,"settingsPage"),"Save preferences");
                    string installRoot=Field<TextBox>(form,"install").Text;
                    var prefs=UpdatePreferences.Load(installRoot);Need(prefs.ModeChosen&&!prefs.AutomaticChecks&&!prefs.AutomaticApply,"Manual mode persistence: "+Field<Label>(form,"notice").Text);
                    Call(form,"ShowUpdateSettings");Click(Field<Control>(form,"settingsPage"),"Check for updates");WaitUpdate(form);Page(form,"updatePage");
                    Need(Field<Label>(form,"updateMessage").Text.Contains("0.3.0"),"Available release missing");
                    Snapshot(form,output,"update");
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
                    Field<Label>(form,"notice").Text="";
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Snapshot(form,output,"home");Click(Field<Control>(form,"homePage"),"Online");Snapshot(form,output,"online-choice");
                    Click(Field<Control>(form,"onlineChoicePage"),"Host");Page(form,"onlinePage");
                    Need(Field<NumericUpDown>(form,"port").Value==7777&&Field<ComboBox>(form,"onlineCharacter").Items.Count==2,"Online defaults/support");Snapshot(form,output,"host");
                    Click(Field<Control>(form,"onlinePage"),"Back");Click(Field<Control>(form,"onlineChoicePage"),"Join");
                    Need(Field<Control>(form,"joinAddressRow").Visible&&!Field<Control>(form,"hostAddressRow").Visible,"Join layout");Snapshot(form,output,"join");
                    string endpoint="[fd7a:115c:a1e0::1]:8123";int selectedPort=7777;LauncherForm.ParseEndpoint(ref endpoint,ref selectedPort);Need(endpoint=="fd7a:115c:a1e0::1"&&selectedPort==8123,"IPv6 endpoint parse");
                    endpoint="192.0.2.1:9123";LauncherForm.ParseEndpoint(ref endpoint,ref selectedPort);Need(endpoint=="192.0.2.1"&&selectedPort==9123,"IPv4 endpoint parse");
                    Need(LauncherForm.FriendlyStage("private raw converter diagnostics")==null,"Raw output reached UI");
                    Escape(form);Page(form,"onlineChoicePage");Escape(form);Page(form,"homePage");
                    Button homePlay=Button(Field<Control>(form,"homePage"),"Play Offline");Reach(form,homePlay);Need(form.SelectNextControl(homePlay,true,true,true,true),"Tab navigation failed");Snapshot(form,output,"home-keyboard-focus");
                    foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Snapshot(form,output,"home-dpi-"+dpi);Reach(form,Button(Field<Control>(form,"homePage"),"Setup / repair / add characters"));}
                    Dpi(form,96);form.ClientSize=new Size(1536,1024);
                    Field<Label>(form,"readyStatus").Text="Mario + Octane: ready\nOffline extras: none selected\nCar sounds: game audio fallback";
                    Field<Label>(form,"notice").Text="";Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"readyPage"));Snapshot(form,output,"ready");
                    Call(form,"ShowUpdateSettings");Snapshot(form,output,"settings");
                    Field<RadioButton>(form,"extrasNo").Checked=true;Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"extrasPage"));Snapshot(form,output,"extras-default");
                    foreach(var c in choices)c.Checked=false;Field<RadioButton>(form,"extrasYes").Checked=true;Snapshot(form,output,"extras-collapsed");
                    foreach(var c in choices)c.Checked=true;Snapshot(form,output,"extras-expanded-top");Reach(form,Button(Field<Control>(form,"extrasPage"),"Install / resume"));Snapshot(form,output,"extras-expanded-bottom");
                    // Exercise cancellation callbacks without a helper, game, or network request.
                    Set(form,"running",true);Set(form,"cancellationAvailable",true);Field<Panel>(form,"pageHost").Enabled=false;
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"progressPage"));Field<Label>(form,"progressText").Text="Extracting and checking your selected assets...";
                    Button cancel=Field<Button>(form,"cancelOperation");cancel.Text="Cancel setup";cancel.Visible=cancel.Enabled=true;Application.DoEvents();Snapshot(form,output,"progress");
                    Need(cancel.Enabled&&cancel.Visible&&!Field<Panel>(form,"pageHost").Enabled,"Cancellation disabled with page contents");cancel.PerformClick();Need(Field<bool>(form,"cancelRequested")&&!cancel.Enabled,"Setup cancel callback failed");
                    Set(form,"running",false);Set(form,"cancellationAvailable",false);Set(form,"cancelRequested",false);cancel.Visible=false;Field<Panel>(form,"pageHost").Enabled=true;
                    using(var token=new System.Threading.CancellationTokenSource()){Set(form,"updateCancellation",token);cancel.Visible=cancel.Enabled=true;cancel.PerformClick();Need(token.IsCancellationRequested,"Update cancel callback failed");Set(form,"updateCancellation",null);cancel.Visible=false;}
                    Field<Label>(form,"notice").Text="";
                    form.Size=form.MinimumSize;
                    foreach(string page in new string[]{"homePage","locationPage","setupPage","extrasPage","readyPage","onlinePage","settingsPage","updatePage","failurePage"}){Call(form,"ShowPage",Field<FlowLayoutPanel>(form,page));Snapshot(form,output,page+"-minimum");}
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"extrasPage"));Reach(form,Button(Field<Control>(form,"extrasPage"),"Install / resume"));Snapshot(form,output,"extras-minimum-bottom");
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"setupPage"));Reach(form,Button(Field<Control>(form,"setupPage"),"Next"));Escape(form);Page(form,"locationPage");
                    string longFailure=String.Join(" ",new string[12]).Replace(" ","This source needs repair. Check the original file and available disk space before trying again. ");
                    Field<Label>(form,"failureText").Text=LauncherForm.PlainFailure(longFailure);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"failurePage"));Need(Field<Label>(form,"failureText").Height>164*960/1536,"Long recovery copy did not expand");Snapshot(form,output,"recovery-long-minimum");Reach(form,Button(Field<Control>(form,"failurePage"),"Retry / resume"));
                    form.ClientSize=new Size(1280,720);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Reach(form,Button(Field<Control>(form,"homePage"),"Setup / repair / add characters"));Snapshot(form,output,"home-wide-short");
                    AssertSeparate(expected);form.Close();
                }
                File.WriteAllText(Path.Combine(output,"ui-result.json"),new JavaScriptSerializer().Serialize(new{passed=true,checks=checks,separate_desktop=expected,input_desktop=InputDesktop(),game_started=false,helper_started=
                    false
                ,address_detection_used=false,dpi_message_scales=new[]{96,120,144,192},real_monitor_switch_tested=false}));return 0;
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
                    60000
                ){TerminateProcess(pi.process,99);throw new TimeoutException("Only the test child was terminated");}}
                uint result;GetExitCodeProcess(pi.process,out result);Need(!childFocused&&InputDesktop()!=name,"Test took input desktop");
                File.WriteAllText(Path.Combine(output,"desktop-isolation.json"),new JavaScriptSerializer().Serialize(new{child_exit=result,child_foreground_seen=childFocused,input_desktop_before=input,input_desktop_after=InputDesktop(),test_desktop=name,never_called_switch_desktop=true}));
                Console.WriteLine("Separate-desktop launcher UI test exit="+result+"; no input-desktop switch; child foreground="+childFocused);return (int)result;
            } finally {if(pi.thread!=IntPtr.Zero)CloseHandle(pi.thread);if(pi.process!=IntPtr.Zero)CloseHandle(pi.process);CloseDesktop(desktop);}
        }
    }
}
