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
        [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr window,IntPtr dc,uint flags);
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
        static void Page(LauncherForm form,string name) {Need(Field<Control>(form,name).Visible,"Wrong page: "+name+"; notice="+Field<Label>(form,"notice").Text+"; failure="+Field<Label>(form,"failureText").Text+"; update="+Field<Label>(form,"updateMessage").Text);}
        // DrawToBitmap composites transparent children differently and hid the
        // reported black radio/checkbox bars. Capture the native window instead.
        static void Snapshot(LauncherForm form,string output,string name) {WindowSnapshot(form,output,name);}
        static void WindowSnapshot(LauncherForm form,string output,string name) {
            form.Refresh();Application.DoEvents();using(var bitmap=new Bitmap(form.Width,form.Height))using(var graphics=Graphics.FromImage(bitmap)){
                IntPtr dc=graphics.GetHdc();try{Need(PrintWindow(form.Handle,dc,0),"Window rendering failed");}finally{graphics.ReleaseHdc(dc);}bitmap.Save(Path.Combine(output,name+".png"),ImageFormat.Png);
            }
        }
        static Control TextControl(Control parent,string text){foreach(Control c in parent.Controls){if(c.Text==text)return c;Control found=TextControl(c,text);if(found!=null)return found;}return null;}
        static void ChoicePaint(LauncherForm form,string output,string name){
            Field<Label>(form,"notice").Text="";Call(form,"ShowUpdateSettings");
            var auto=Field<RadioButton>(form,"automaticUpdates");var manual=Field<RadioButton>(form,"manualUpdates");
            Control autoHelp=TextControl(auto.Parent,"Check at startup. Choose Update now or Later."),manualHelp=TextControl(auto.Parent,"No startup update checks. Check when you choose.");
            Need(auto.Bottom<autoHelp.Top&&autoHelp.Bottom<manual.Top&&manual.Bottom<manualHelp.Top,"Update choices overlap their descriptions");
            foreach(bool selected in new[]{false,true}){
                auto.Checked=selected;manual.Checked=!selected;auto.Focus();form.Refresh();Application.DoEvents();
                using(var bitmap=new Bitmap(form.Width,form.Height))using(var graphics=Graphics.FromImage(bitmap)){
                    IntPtr dc=graphics.GetHdc();try{Need(PrintWindow(form.Handle,dc,0),"Native choice rendering failed");}finally{graphics.ReleaseHdc(dc);}
                    foreach(Control choice in new Control[]{auto,manual,Field<CheckBox>(form,"desktopShortcut"),Field<CheckBox>(form,"menuShortcut")}){
                        Point point=form.PointToClient(choice.PointToScreen(new Point(choice.Width-15,choice.Height/2)));
                        Color color=bitmap.GetPixel(point.X,point.Y);Need(color.B>20&&color.B>color.R,"Native choice background is black: "+choice.Text+" @ "+form.ClientSize);
                    }
                    bitmap.Save(Path.Combine(output,name+(selected?"-automatic":"-manual")+".png"),ImageFormat.Png);
                }
            }
            Reach(form,Button(Field<Control>(form,"settingsPage"),"Save preferences"));Escape(form);
        }
        static Dictionary<string,object> Report(bool playable,bool engine,params string[] ready){return new Dictionary<string,object>{{"playable",playable},{"engine",engine},{"ready",ready},{"sm64",true},{"audio",false},{"errors",new Dictionary<string,object>()},{"detected",new string[0]}};}
        static void StartupRoutes(LauncherForm form,string output,FakeUpdateTransport transport){
            string root=Field<TextBox>(form,"install").Text;string preferences=UpdatePreferences.PathFor(root);
            var saved=new UpdatePreferences{Configured=true,ModeChosen=true,AutomaticChecks=false,AutomaticApply=false,DesktopShortcut=true,StartMenuShortcut=false,PausedVersion="0.3.0"};saved.Save(root);
            string before=File.ReadAllText(preferences);int reads=transport.Reads;
            Call(form,"StartupUpdates");Page(form,"locationPage");Need(transport.Reads==reads&&!Field<bool>(form,"installationReady"),"Preferences alone bypassed Setup");
            foreach(var report in new[]{Report(false,false),Report(false,false,"octane"),Report(true,false,"octane"),Report(true,true)}){
                Set(form,"lastReport",report);Call(form,"CompleteInspection",true);Page(form,"locationPage");Need(!Field<bool>(form,"installationReady")&&transport.Reads==reads,"Incomplete status entered Play/update");
            }
            Set(form,"lastReport",Report(false,false,"octane"));Call(form,"CompleteInspection",false);Page(form,"setupPage");Need(!Field<Button>(form,"sourceNext").Enabled,"Repair bypassed source selection");
            Set(form,"lastReport",Report(true,true,"octane"));Call(form,"CompleteInspection",true);Page(form,"homePage");Need(File.ReadAllText(preferences)==before&&Field<TextBox>(form,"install").Text==root&&transport.Reads==reads,"Configured migration changed preferences/path or made a manual update request");
            WindowSnapshot(form,output,"startup-configured-play");
            saved.AutomaticChecks=true;saved.AutomaticApply=true;saved.Save(root);before=File.ReadAllText(preferences);
            Call(form,"CompleteInspection",true);WaitUpdate(form);Page(form,"homePage");Need(transport.Reads>reads&&File.ReadAllText(preferences)==before,"Notification startup changed legacy preferences");
            // Legacy check-only consent must never become automatic install consent.
            File.WriteAllText(preferences,"{\"schema\":1,\"configured\":true,\"automatic_checks\":true,\"desktop_shortcut\":true,\"start_menu_shortcut\":false}");before=File.ReadAllText(preferences);reads=transport.Reads;
            Call(form,"CompleteInspection",true);WaitUpdate(form);Page(form,"homePage");Need(!UpdatePreferences.Load(root).AutomaticApply,"Legacy consent was inferred");Need(File.ReadAllText(preferences)==before&&transport.Reads>reads,"Legacy notification preference was not respected");
            WindowSnapshot(form,output,"startup-legacy-play");
            reads=transport.Reads;File.Delete(preferences);Call(form,"CompleteInspection",true);WaitUpdate(form);Page(form,"homePage");Need(!File.Exists(preferences)&&transport.Reads>reads,"Default notifications wrote preferences or did not check");reads=transport.Reads;WindowSnapshot(form,output,"startup-verified-play");
            Call(form,"ShowReadyPage");Page(form,"readyPage");
            Field<RadioButton>(form,"readyManual").Checked=true;Click(Field<Control>(form,"readyPage"),"Open launcher");Page(form,"homePage");Need(UpdatePreferences.Load(root).ModeChosen,"Setup completion did not persist explicit choice");Call(form,"CompleteInspection",true);Page(form,"homePage");
            // Existing but interrupted data must run inspection, not infer readiness.
            Directory.CreateDirectory(Commands.DataDirectory(root));Call(form,"StartupUpdates");WaitUpdate(form);Page(form,"failurePage");Need(!Field<bool>(form,"installationReady")&&transport.Reads==reads,"Failed inspection retained stale readiness");Click(Field<Control>(form,"failurePage"),"Back / change source");Page(form,"setupPage");
        }
        static void AssertSeparate(string expected) {Need(DesktopName(GetThreadDesktop(GetCurrentThreadId()))==expected,"Wrong thread desktop; refusing UI");Need(InputDesktop()!=expected,"Test desktop is active; refusing UI");}
        static void WaitSources(LauncherForm form){var watch=Stopwatch.StartNew();while((Field<bool>(form,"sourceValidationRunning")||Field<System.Windows.Forms.Timer>(form,"sourceDelay").Enabled)&&watch.ElapsedMilliseconds<10000){Application.DoEvents();Thread.Sleep(10);}Need(!Field<bool>(form,"sourceValidationRunning")&&!Field<System.Windows.Forms.Timer>(form,"sourceDelay").Enabled,"Source validation did not finish");Application.DoEvents();}
        static void SourceSelection(LauncherForm form,string output){
            var fixture=SourceFixture.Create(Path.Combine(output,"source-fixture"));form.CheckSources=fixture.Validator.Check;
            var rom=Field<TextBox>(form,"rom");var game=Field<TextBox>(form,"game");var next=Field<Button>(form,"sourceNext");
            Field<Label>(form,"notice").Text="";Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"setupPage"));rom.Text="";game.Text="";WaitSources(form);Need(!next.Enabled,"Empty selections enabled Next");Snapshot(form,output,"sources-empty");
            rom.Text=fixture.InvalidRom;game.Text=fixture.Game;Need(!next.Enabled,"Editing retained old validation");WaitSources(form);Need(!next.Enabled,"Existing invalid ROM enabled Next");Snapshot(form,output,"sources-invalid-rom");
            rom.Text=fixture.Rom;game.Text=output;WaitSources(form);Need(!next.Enabled,"Existing wrong folder enabled Next");Snapshot(form,output,"sources-invalid-folder");
            game.Text=fixture.Game;WaitSources(form);Need(next.Enabled,"Verified source pair did not enable Next");Reach(form,next);Snapshot(form,output,"sources-valid");
            foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);Snapshot(form,output,"sources-valid-dpi-"+dpi);rom.Text=fixture.InvalidRom;WaitSources(form);Need(!next.Enabled,"Invalid ROM remained enabled at DPI "+dpi);Snapshot(form,output,"sources-invalid-dpi-"+dpi);rom.Text=fixture.Rom;WaitSources(form);Need(next.Enabled,"Valid pair failed at DPI "+dpi);}
            // A delayed obsolete success must not enable a new invalid selection.
            using(var release=new ManualResetEvent(false))using(var started=new ManualResetEvent(false)){
                form.CheckSources=delegate(string r,string g,CancellationToken token){started.Set();release.WaitOne(5000);return new SourceValidationResult{RomValid=true,GameValid=true,RomMessage="old",GameMessage="old"};};
                Call(form,"ValidateSelectedSources",false);Need(started.WaitOne(2000),"Validation worker did not start");rom.Text=fixture.InvalidRom;form.CheckSources=fixture.Validator.Check;release.Set();WaitSources(form);Need(!next.Enabled&&Field<Label>(form,"romFeedback").Text!="old","Obsolete success replaced current failure");
            }
            rom.Text=fixture.Rom;game.Text=fixture.Game;WaitSources(form);Need(next.Enabled,"Valid source retry failed");
            using(var release=new ManualResetEvent(false))using(var started=new ManualResetEvent(false)){
                form.CheckSources=delegate(string r,string g,CancellationToken token){started.Set();release.WaitOne(5000);return new SourceValidationResult{RomValid=true,GameValid=true,RomMessage="old",GameMessage="old"};};
                next.PerformClick();Need(started.WaitOne(2000),"Next validation worker did not start");Click(Field<Control>(form,"setupPage"),"Back");release.Set();Application.DoEvents();Page(form,"locationPage");Need(!Field<bool>(form,"sourceNextRequested"),"Back retained a pending Next action");
            }
            form.CheckSources=fixture.Validator.Check;Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"setupPage"));WaitSources(form);Need(next.Enabled,"Source validation did not resume after Back");
            // File bytes can change without a TextChanged event. Next rechecks.
            File.WriteAllText(fixture.Rom,"changed since validation");next.PerformClick();WaitSources(form);Need(!next.Enabled&&!Field<bool>(form,"running"),"Next used stale file validation");Page(form,"setupPage");
            fixture=SourceFixture.Create(Path.Combine(output,"source-fixture-next"));form.CheckSources=fixture.Validator.Check;rom.Text=fixture.Rom;game.Text=fixture.Game;WaitSources(form);next.PerformClick();WaitSources(form);WaitUpdate(form);Page(form,"failurePage");
            // The test payload is intentionally absent; the real preflight route
            // reaches recovery without launching any helper or modifying sources.
            Need(File.Exists(fixture.Rom),"Selection changed source data");Click(Field<Control>(form,"failurePage"),"Back / change source");WaitSources(form);Need(next.Enabled,"Back did not revalidate selected sources");
        }
        static void ControlsFit(Control root,Panel host){
            foreach(Control c in root.Controls){if(!c.Visible)continue;var scroll=c as ScrollableControl;if(scroll!=null)Need(!scroll.HorizontalScroll.Visible&&!scroll.VerticalScroll.Visible,"Scrollbar visible: "+c.Name);
                Rectangle r=host.RectangleToClient(c.RectangleToScreen(c.ClientRectangle));Need(r.Left>=-1&&r.Top>=-1&&r.Right<=host.ClientSize.Width+1&&r.Bottom<=host.ClientSize.Height+1,"Clipped control: "+c.Text+" "+r+" viewport="+host.ClientSize);
                if(c.HasChildren)ControlsFit(c,host);
            }
        }
        static void AllPagesFit(LauncherForm form,string output,string size){
            Field<Label>(form,"notice").Text="";Field<Label>(form,"readyStatus").Text="Mario + Octane are ready.";
            Field<RadioButton>(form,"extrasYes").Checked=true;foreach(var c in Field<CheckBox[]>(form,"extraChoices"))c.Checked=true;
            foreach(string name in new[]{"homePage","locationPage","setupPage","extrasPage","readyPage","onlineChoicePage","onlinePage","settingsPage","updatePage","failurePage","progressPage"}){
                Call(form,"ShowPage",Field<FlowLayoutPanel>(form,name));if(name=="setupPage")WaitSources(form);
                Panel host=Field<Panel>(form,"pageHost");Need(!host.AutoScroll&&!host.HorizontalScroll.Visible&&!host.VerticalScroll.Visible,"Page scrollbars are enabled");ControlsFit(Field<Control>(form,name),host);Snapshot(form,output,"fit-"+size+"-"+name);
            }
        }
        static void WaitUpdate(LauncherForm form) {
            Stopwatch timer=Stopwatch.StartNew();while((Field<bool>(form,"running")||Field<bool>(form,"checkingUpdates"))&&timer.ElapsedMilliseconds<15000){Application.DoEvents();Thread.Sleep(20);}
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
                    form.AutoAppearanceMigration=false;form.UpdateTransport = transport;
                    // Seed only this synthetic root so real running games need not be stopped.
                    string fixtureRoot=Field<TextBox>(form,"install").Text;
                    // This deliberately incomplete version directory exercises
                    // verification/recovery without depending on global disk space.
                    Directory.CreateDirectory(Installer.Destination(fixtureRoot,PayloadInfo.ZipSha256));
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
                    Need(Field<RadioButton>(form,"readyAuto").Checked&&!Field<RadioButton>(form,"readyManual").Checked,"Notifications are not enabled by default");
                    Field<RadioButton>(form,"readyAuto").Checked=false;
                    Click(Field<Control>(form,"readyPage"),"Open launcher");Page(form,"readyPage");
                    Need(Field<Label>(form,"notice").Text.Contains("Choose"),"No update choice validation");
                    Snapshot(form,output,"05-ready");
                    Call(form,"ShowUpdateSettings");
                    form.ClientSize=new Size(1152,768);WindowSnapshot(form,output,"settings-window-default");form.ClientSize=new Size(1536,1024);
                    Snapshot(form,output,"settings-first-run");
                    Field<RadioButton>(form,"manualUpdates").Checked=true;Click(Field<Control>(form,"settingsPage"),"Save preferences");
                    string installRoot=Field<TextBox>(form,"install").Text;
                    var prefs=UpdatePreferences.Load(installRoot);Need(prefs.ModeChosen&&!prefs.AutomaticChecks&&!prefs.AutomaticApply,"Manual mode persistence: "+Field<Label>(form,"notice").Text);
                    Call(form,"ShowUpdateSettings");Click(Field<Control>(form,"settingsPage"),"Check for updates");WaitUpdate(form);Page(form,"updatePage");
                    Need(Field<Label>(form,"updateMessage").Text.Contains("0.3.0"),"Available release missing");
                    Snapshot(form,output,"update");
                    Need(!Field<Button>(form,"useInstalled").Visible&&!Field<Button>(form,"retryUpdate").Visible,"Normal update page has extra buttons");
                    transport.Offline=true;Call(form,"CheckUpdates",false);WaitUpdate(form);Page(form,"updatePage");
                    Need(Field<Label>(form,"updateMessage").Text.Contains("retained"),"Offline fallback missing");transport.Offline=false;
                    Call(form,"ShowUpdateSettings");Field<RadioButton>(form,"automaticUpdates").Checked=true;Click(Field<Control>(form,"settingsPage"),"Save preferences");
                    prefs=UpdatePreferences.Load(installRoot);Need(prefs.ModeChosen&&!prefs.AutomaticApply&&prefs.AutomaticChecks,"Notification preference not persisted");
                    transport.Corrupt=true;form.UpdateGameActive=delegate{return false;};
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Field<HashSet<string>>(form,"announcedUpdates").Clear();Call(form,"CheckUpdates",true);WaitUpdate(form);Page(form,"homePage");Need(Field<bool>(form,"notificationVisible"),"Missing update notification");Field<Button>(form,"notifyUpdate").PerformClick();Page(form,"updatePage");
                    Need(transport.Downloads==0,"Notification installed without Update now");Snapshot(form,output,"update-notification");
                    Click(Field<Control>(form,"updatePage"),"Download update and restart");WaitUpdate(form);Page(form,"updatePage");
                    Need(transport.Downloads==1&&UpdatePreferences.Load(installRoot).PausedVersion=="0.3.0","Explicit update did not verify/pause corrupt release");transport.Corrupt=false;
                    Click(Field<Control>(form,"updatePage"),"Back");Page(form,"locationPage");Set(form,"installationReady",true);
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Call(form,"CheckUpdates",true);WaitUpdate(form);Page(form,"homePage");
                    Need(transport.Downloads==1,"Dismissed update repeated or auto-installed");
                    Call(form,"CheckUpdates",false);WaitUpdate(form);Page(form,"updatePage");Need(form.CancelButton==null,"Normal update page added an alternate action");Call(form,"DismissUpdate");Page(form,"homePage");
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
                    foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Snapshot(form,output,"home-dpi-"+dpi);Reach(form,Field<Button>(form,"setupRepair"));}
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
                    Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"setupPage"));Need(!Field<Button>(form,"sourceNext").Enabled,"Blank sources enabled Next");Reach(form,Button(Field<Control>(form,"setupPage"),"Back"));Escape(form);Page(form,"locationPage");
                    string longFailure=String.Join(" ",new string[12]).Replace(" ","This source needs repair. Check the original file and available disk space before trying again. ");
                    Field<Label>(form,"failureText").Text=LauncherForm.PlainFailure(longFailure);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"failurePage"));Need(Field<Label>(form,"failureText").Height>164*960/1536,"Long recovery copy did not expand");Snapshot(form,output,"recovery-long-minimum");Reach(form,Button(Field<Control>(form,"failurePage"),"Retry / resume"));
                    form.ClientSize=new Size(1280,720);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Reach(form,Field<Button>(form,"setupRepair"));Snapshot(form,output,"home-wide-short");
                    Dpi(form,96);form.ClientSize=new Size(1152,768);ChoicePaint(form,output,"settings-native-default");
                    foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);ChoicePaint(form,output,"settings-native-dpi-"+dpi);}
                    Dpi(form,96);form.ClientSize=new Size(960,640);ChoicePaint(form,output,"settings-native-minimum");
                    form.ClientSize=new Size(1536,1024);StartupRoutes(form,output,transport);
                    SourceSelection(form,output);
                    AddCharacters(form,output);
                    InstalledRoutes(form,output);
                    foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);AllPagesFit(form,output,"dpi-"+dpi);}
                    Dpi(form,96);form.ClientSize=new Size(960,640);AllPagesFit(form,output,"minimum");
                    form.ClientSize=new Size(1280,720);AllPagesFit(form,output,"wide");
                    AssertSeparate(expected);form.Close();
                }
                Lifecycle(output);AppearanceFlows(output);AutomaticAppearanceFlows(output);
                File.WriteAllText(Path.Combine(output,"ui-result.json"),new JavaScriptSerializer().Serialize(new{passed=true,checks=checks,separate_desktop=expected,input_desktop=InputDesktop(),game_started=false,helper_started=
                    false
                ,synthetic_operation_children_started=true,address_detection_used=false,dpi_message_scales=new[]{96,120,144,192},real_monitor_switch_tested=false}));return 0;
            } catch(Exception e){File.WriteAllText(Path.Combine(output,"ui-error.txt"),e.ToString());return 1;}
        }

        static void PumpUntil(Func<bool> condition,string message){var watch=Stopwatch.StartNew();while(!condition()&&watch.ElapsedMilliseconds<12000){Application.DoEvents();Thread.Sleep(20);}Need(condition(),message);Application.DoEvents();}
        static int OperationFixture(string root,string mode,string cancel){
            Directory.CreateDirectory(root);File.WriteAllText(Path.Combine(root,"started-"+mode),Process.GetCurrentProcess().Id.ToString());
            if(mode=="play"){
                if(File.Exists(Path.Combine(root,"fail")))return 9;
                File.WriteAllText(Path.Combine(root,"game-running"),"ready");
                try{var watch=Stopwatch.StartNew();while(!File.Exists(Path.Combine(root,"stop"))&&watch.ElapsedMilliseconds<30000)Thread.Sleep(20);}
                finally{File.Delete(Path.Combine(root,"game-running"));}
            }else if(mode.StartsWith("appearance-")){
                if(mode!="appearance-status"&&mode!="appearance-migrate"){
                    var watch=Stopwatch.StartNew();while(!File.Exists(Path.Combine(root,"finish"))&&watch.ElapsedMilliseconds<12000){if(File.Exists(cancel))return 130;Thread.Sleep(20);}
                    if(File.Exists(Path.Combine(root,"fail"))){Console.Error.WriteLine("Stopped: This Rocket League folder is missing supported packages. Current appearance is unchanged.");return 2;}
                    if(mode=="appearance-apply"){File.WriteAllText(Path.Combine(root,"material-active"),"fixture");File.Delete(Path.Combine(root,"material-saved"));}
                    else{File.Delete(Path.Combine(root,"material-active"));File.WriteAllText(Path.Combine(root,"material-saved"),"fixture");}
                }
                if(mode=="appearance-migrate"&&!File.Exists(Path.Combine(root,"material-saved"))&&!File.Exists(Path.Combine(root,"missing-source")))File.WriteAllText(Path.Combine(root,"material-active"),"auto fixture");
                bool active=File.Exists(Path.Combine(root,"material-active")),reusable=File.Exists(Path.Combine(root,"material-saved"));
                Console.WriteLine(new JavaScriptSerializer().Serialize(new{needs_attention=File.Exists(Path.Combine(root,"missing-source")),deferred=false,active=active,reusable=reusable,can_apply=!active,can_undo=active,source="C:\\Synthetic owned Rocket League",message=active?"Improved body textures and dark windows are active. Revert restores the previous plain appearance.":reusable?"You chose the previous appearance. Switch back to reuse your verified textures without a source or download.":"Supported local textures found. Ready to upgrade the car appearance."}));
            }else if(mode=="setup"){
                var watch=Stopwatch.StartNew();while(!File.Exists(Path.Combine(root,"finish"))&&watch.ElapsedMilliseconds<12000){if(File.Exists(cancel))return 130;Thread.Sleep(20);}
                if(File.Exists(Path.Combine(root,"fail")))return 8;
            }else if(mode=="preflight")Console.WriteLine("{\"valid\":true,\"message\":\"Ready\"}");
            else if(mode=="wizard-status")Console.WriteLine("{\"playable\":true,\"engine\":true,\"ready\":[\"octane\",\"link\"],\"errors\":{},\"detected\":[]}");
            return 0;
        }
        static LauncherForm OperationForm(string root,List<List<string>> commands){
            var form=new LauncherForm(root);form.AutoAppearanceMigration=false;form.UpdateGameActive=delegate{return File.Exists(Path.Combine(root,"game-running"));};
            form.UpdateTransport=FakeUpdateTransport.New("0.3.0",new byte[]{1,2,3});
            form.PrepareProgramFiles=delegate(string path,Action<string> log){return root;};
            form.GameStarted=delegate(string path,DateTime started){return File.Exists(Path.Combine(root,"game-running"));};
            form.OperationStartInfo=delegate(string path,string data,string cancel,List<string> args){commands.Add(new List<string>(args));return new ProcessStartInfo{FileName=Assembly.GetExecutingAssembly().Location,Arguments="--operation-fixture "+Commands.Quote(root)+" "+Commands.Quote(args[0])+" "+Commands.Quote(cancel??"none"),UseShellExecute=false,CreateNoWindow=true,RedirectStandardInput=true,RedirectStandardOutput=true,RedirectStandardError=true};};
            form.Show();Application.DoEvents();Set(form,"installationReady",true);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));return form;
        }
        static void AddCharacters(LauncherForm form,string output){
            Set(form,"installationReady",true);Field<HashSet<string>>(form,"readyCharacters").Clear();Field<HashSet<string>>(form,"readyCharacters").Add("octane");
            foreach(var choice in Field<CheckBox[]>(form,"extraChoices"))choice.Checked=false;
            foreach(var source in Field<TextBox[]>(form,"extraSources"))source.Text="";
            Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Click(Field<Control>(form,"homePage"),"Add characters");Page(form,"extrasPage");
            Need(Field<bool>(form,"addingCharacters")&&!Field<RadioButton>(form,"extrasNo").Visible,"Add characters still uses base wizard");
            Click(Field<Control>(form,"extrasPage"),"Install / resume");Need(Field<Label>(form,"notice").Text.Contains("Choose at least"),"No selection validation");
            Field<CheckBox[]>(form,"extraChoices")[0].Checked=true;Click(Field<Control>(form,"extrasPage"),"Install / resume");Page(form,"extrasPage");Need(!Field<bool>(form,"running")&&Field<Label>(form,"notice").Text.Contains("original ROM"),"Missing optional source rerouted base setup");
            Field<HashSet<string>>(form,"readyCharacters").Add("link");Call(form,"BuildSetupQueue");var queue=Field<Queue<List<string>>>(form,"setupQueue");Need(queue.Count==1&&queue.Peek().Contains("link")&&!queue.Peek().Contains("--sm64")&&!queue.Peek().Contains("octane"),"Add-only queue reinstalled core assets");queue.Clear();
            Dpi(form,96);form.ClientSize=new Size(1536,1024);Snapshot(form,output,"add-characters");form.ClientSize=new Size(960,640);Need(form.ClientSize==new Size(960,640),"Add-character minimum fixture was not 960x640");ControlsFit(Field<Control>(form,"extrasPage"),Field<Panel>(form,"pageHost"));Snapshot(form,output,"add-characters-minimum");Escape(form);Page(form,"homePage");Need(!Field<bool>(form,"addingCharacters"),"Back did not end add-only flow");
            Set(form,"installationReady",false);Call(form,"ShowAddCharacters");Page(form,"locationPage");
        }
        static void Lifecycle(string output){
            string verified=Path.Combine(output,"verify-installed");Directory.CreateDirectory(Installer.Destination(verified,PayloadInfo.ZipSha256));var verifyCommands=new List<List<string>>();
            using(var form=OperationForm(verified,verifyCommands)){
                Field<TextBox>(form,"rom").Text="previous ROM selection";Field<TextBox>(form,"game").Text="previous Rocket League selection";
                string preferences=UpdatePreferences.PathFor(verified);new UpdatePreferences{AutomaticChecks=false}.Save(verified);string before=File.ReadAllText(preferences);
                Field<Button>(form,"setupRepair").PerformClick();Page(form,"failurePage");Need(Field<bool>(form,"repairOverview"),"Installed repair launched wizard automatically");Click(Field<Control>(form,"failurePage"),"Verify installation");WaitUpdate(form);Page(form,"extrasPage");
                Need(verifyCommands.Count==1&&verifyCommands[0][0]=="wizard-status"&&Field<bool>(form,"addingCharacters"),"Verified repair repeated setup instead of Characters");
                Need(Field<TextBox>(form,"rom").Text=="previous ROM selection"&&Field<TextBox>(form,"game").Text=="previous Rocket League selection"&&File.ReadAllText(preferences)==before,"Installed verification changed selections/settings");form.Close();
            }
            string adding=Path.Combine(output,"add-flow");Directory.CreateDirectory(adding);File.WriteAllText(Path.Combine(adding,"finish"),"done");
            var addCommands=new List<List<string>>();using(var form=OperationForm(adding,addCommands)){
                var prefs=new UpdatePreferences{AutomaticChecks=false,Configured=true};prefs.Save(adding);string before=File.ReadAllText(UpdatePreferences.PathFor(adding));
                Field<HashSet<string>>(form,"readyCharacters").Add("octane");Field<HashSet<string>>(form,"readyCharacters").Add("link");Call(form,"ShowAddCharacters");Field<CheckBox[]>(form,"extraChoices")[0].Checked=true;
                Click(Field<Control>(form,"extrasPage"),"Install / resume");WaitUpdate(form);Page(form,"homePage");Need(Field<Label>(form,"notice").Text=="Characters added."&&!Field<bool>(form,"addingCharacters"),"Direct add did not finish on Play");
                Need(addCommands.Count==3&&addCommands[0][0]=="preflight"&&addCommands[1].Contains("link")&&addCommands[2][0]=="wizard-status","Direct add operation sequence changed");foreach(var command in addCommands)Need(!command.Contains("--sm64")&&!command.Contains("octane"),"Direct add requested base sources");Need(File.ReadAllText(UpdatePreferences.PathFor(adding))==before,"Direct add rewrote preferences");form.Close();
            }
            string notifying=Path.Combine(output,"notification");using(var form=OperationForm(notifying,new List<List<string>>())){
                var transport=(FakeUpdateTransport)form.UpdateTransport;transport.WaitForCancel=true;Call(form,"CheckUpdates",true);
                Need(!Field<bool>(form,"running")&&!Field<Control>(form,"progressPage").Visible&&Field<Panel>(form,"pageHost").Enabled,"Background update check blocked Play");
                form.UpdateGameActive=delegate{return true;};transport.WaitForCancel=false;WaitUpdate(form);Need(Field<bool>(form,"pendingUpdateNotice")&&!Field<bool>(form,"notificationVisible"),"Update notification interrupted running game");
                form.UpdateGameActive=delegate{return false;};Call(form,"PresentUpdate",true);Need(Field<bool>(form,"notificationVisible"),"Deferred notification lost");Snapshot(form,output,"play-update-notification");
                Field<Button>(form,"dismissNotification").PerformClick();Call(form,"CheckUpdates",true);WaitUpdate(form);Need(!Field<bool>(form,"notificationVisible")&&transport.Downloads==0,"Later repeated notification or installed update");
                new UpdatePreferences{AutomaticChecks=false}.Save(notifying);int reads=transport.Reads;Call(form,"CheckUpdates",true);Need(transport.Reads==reads,"Opt-out made a background request");form.Close();
            }
            string pending=Path.Combine(output,"close-launch");Directory.CreateDirectory(pending);using(var gate=new ManualResetEvent(false))using(var entered=new ManualResetEvent(false))using(var form=OperationForm(pending,new List<List<string>>())){
                form.PrepareProgramFiles=delegate(string path,Action<string> log){entered.Set();gate.WaitOne(10000);return pending;};
                Click(Field<Control>(form,"homePage"),"Play Offline");PumpUntil(delegate{return entered.WaitOne(0);},"Pending launch never began");form.Close();Need(form.IsDisposed,"Pending game launch blocked window close");gate.Set();
                PumpUntil(delegate{return File.Exists(Path.Combine(pending,"game-running"));},"Closing launcher canceled pending game");File.WriteAllText(Path.Combine(pending,"stop"),"stop");PumpUntil(delegate{return !File.Exists(Path.Combine(pending,"game-running"));},"Pending fake game did not clean up");
            }
            string root=Path.Combine(output,"lifecycle");var commands=new List<List<string>>();Directory.CreateDirectory(root);
            using(var form=OperationForm(root,commands)){
                Click(Field<Control>(form,"homePage"),"Play Offline");Need(!Field<Control>(form,"progressPage").Visible,"Game launch displayed a loading page");
                PumpUntil(delegate{return Field<bool>(form,"gameSessionActive");},"Game never entered running state");
                Need(!Field<bool>(form,"running")&&!Field<ProgressBar>(form,"progress").Visible&&Field<Label>(form,"notice").Text=="Game running","Game retained busy state");
                Click(Field<Control>(form,"homePage"),"Play Offline");Need(commands.Count==1,"Repeated click launched a second game");Field<Label>(form,"notice").Text="Game running";Snapshot(form,output,"game-running-fixture");
                using(OperationLease lease=OperationLease.Acquire(root)){} // Released after actual child signal.
                form.Close();Application.DoEvents();Need(form.IsDisposed&&File.Exists(Path.Combine(root,"game-running")),"Close killed the launched game");
                using(var reopened=OperationForm(root,new List<List<string>>())){Need(File.Exists(Path.Combine(root,"game-running")),"Reopen interrupted game");reopened.Close();}
                File.WriteAllText(Path.Combine(root,"stop"),"stop");PumpUntil(delegate{return !File.Exists(Path.Combine(root,"game-running"));},"Owned fake game did not exit");
            }
            string failed=Path.Combine(output,"launch-failure");Directory.CreateDirectory(failed);File.WriteAllText(Path.Combine(failed,"fail"),"fail");
            using(var form=OperationForm(failed,new List<List<string>>())){Click(Field<Control>(form,"homePage"),"Play Offline");WaitUpdate(form);Page(form,"failurePage");Need(!Field<ProgressBar>(form,"progress").Visible,"Failed launch left progress");form.Close();}
            foreach(bool close in new[]{false,true}){
                string setup=Path.Combine(output,close?"setup-close":"setup-cancel");Directory.CreateDirectory(setup);bool completed=false;
                using(var form=OperationForm(setup,new List<List<string>>())){
                    Call(form,"BeginOperation",new List<string>{"setup","--character","octane"},true,(Action)delegate{completed=true;});
                    PumpUntil(delegate{return File.Exists(Path.Combine(setup,"started-setup"));},"Setup fixture failed to start");
                    Need(Field<ProgressBar>(form,"progress").Style==ProgressBarStyle.Marquee,"Unknown helper work used fake determinate progress");
                    if(close){form.Close();Need(!form.IsDisposed&&Field<Label>(form,"notice").Text.Contains("safely"),"Close did not wait nonmodally");PumpUntil(delegate{return form.IsDisposed;},"Safe close did not finish");}
                    else{Escape(form);WaitUpdate(form);Page(form,"failurePage");Need(!Field<ProgressBar>(form,"progress").Visible,"Cancel left progress visible");form.Close();}
                    Need(!completed,"Canceled setup continued its queue");
                }
            }
            foreach(bool fail in new[]{false,true}){
                string setup=Path.Combine(output,fail?"setup-failure":"setup-success");Directory.CreateDirectory(setup);File.WriteAllText(Path.Combine(setup,"finish"),"done");if(fail)File.WriteAllText(Path.Combine(setup,"fail"),"fail");bool completed=false;
                using(var form=OperationForm(setup,new List<List<string>>())){Call(form,"BeginOperation",new List<string>{"setup","--character","octane"},true,(Action)delegate{completed=true;Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));});WaitUpdate(form);Need(completed!=fail&&!Field<ProgressBar>(form,"progress").Visible,"Setup success/failure completion is stale");Page(form,fail?"failurePage":"homePage");form.Close();}
            }
            string updating=Path.Combine(output,"update-close");using(var form=OperationForm(updating,new List<List<string>>())){
                var transport=(FakeUpdateTransport)form.UpdateTransport;transport.WaitForCancel=true;Call(form,"CheckUpdates",false);form.Close();PumpUntil(delegate{return form.IsDisposed;},"Update close did not cancel safely");Need(transport.Downloads==0,"Canceled check downloaded update");
            }
            string measured=Path.Combine(output,"measured");using(var form=OperationForm(measured,new List<List<string>>())){
                Set(form,"running",true);Set(form,"operationGeneration",40);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"progressPage"));Call(form,"MeasuredProgress",40,"Measured fixture",25L,100L);
                var bar=Field<ProgressBar>(form,"progress");Need(bar.Style==ProgressBarStyle.Continuous&&bar.Value==250,"Measured 25/100 did not produce 25%");Snapshot(form,output,"measured-progress-fixture");
                Call(form,"MeasuredProgress",39,"Stale",99L,100L);Need(bar.Value==250,"Stale progress changed current operation");Call(form,"MeasuredProgress",40,"Unknown phase",0L,0L);Need(bar.Style==ProgressBarStyle.Marquee,"Unknown phase retained percentage");Call(form,"ClearBusy");Need(!bar.Visible&&bar.Value==0,"Completion retained progress");form.Close();
            }
        }
        static void AutomaticAppearanceFlows(string output){
            foreach(bool missing in new[]{false,true}){
                string root=Path.Combine(output,missing?"auto-appearance-missing":"auto-appearance-compatible");Directory.CreateDirectory(root);
                if(missing)File.WriteAllText(Path.Combine(root,"missing-source"),"fixture");
                var commands=new List<List<string>>();using(var form=OperationForm(root,commands)){
                    // Represent an already registered installation in this synthetic root.
                    // The real running-game registration guard must remain enabled.
                    UpdateStore.Save(root,new LauncherState{active=UpdateStore.Capture(root,UpdateStore.CurrentExe,UpdateBuild.Version)});
                    form.AutoAppearanceMigration=true;new UpdatePreferences{AutomaticChecks=false,Configured=true}.Save(root);
                    string prefs=File.ReadAllText(UpdatePreferences.PathFor(root));commands.Clear();Set(form,"lastReport",Report(true,true,"octane"));Call(form,"CompleteInspection",true);WaitUpdate(form);Page(form,"homePage");
                    Need(commands.Count==1&&commands[0][0]=="appearance-migrate","Normal installed startup did not migrate appearance automatically");
                    Need(Field<bool>(form,"appearanceNeedsAttention")==missing,"Automatic appearance prompt is not prerequisite-specific");
                    Need(Button(Field<Control>(form,"homePage"),"Finish car appearance").Visible==missing,"Unnecessary appearance prompt on compatible installation");
                    Snapshot(form,output,missing?"auto-appearance-needs-source":"auto-appearance-complete");
                    if(missing){
                        foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);ControlsFit(Field<Control>(form,"homePage"),Field<Panel>(form,"pageHost"));Reach(form,Button(Field<Control>(form,"homePage"),"Finish car appearance"));}
                        Dpi(form,96);form.ClientSize=new Size(960,640);Set(form,"notificationVisible",true);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));ControlsFit(Field<Control>(form,"homePage"),Field<Panel>(form,"pageHost"));Snapshot(form,output,"appearance-and-update-prompt-minimum");Set(form,"notificationVisible",false);
                        Click(Field<Control>(form,"homePage"),"Finish car appearance");WaitUpdate(form);Page(form,"appearancePage");Escape(form);Page(form,"homePage");}
                    else{
                        File.Delete(Path.Combine(root,"material-active"));File.WriteAllText(Path.Combine(root,"material-saved"),"user chose revert");
                        Call(form,"StartAppearanceMigration",(Action)null);WaitUpdate(form);Need(!File.Exists(Path.Combine(root,"material-active"))&&!Field<bool>(form,"appearanceNeedsAttention"),"Normal startup overwrote explicit revert choice");
                        File.Delete(Path.Combine(root,"material-saved"));File.WriteAllText(Path.Combine(root,"game-running"),"fixture");int count=commands.Count;
                        Call(form,"StartAppearanceMigration",(Action)null);Need(commands.Count==count&&Field<bool>(form,"appearanceMigrationPending"),"Automatic migration touched running game");File.Delete(Path.Combine(root,"game-running"));
                        Call(form,"StartAppearanceMigration",(Action)null);WaitUpdate(form);Need(File.Exists(Path.Combine(root,"material-active")),"Deferred appearance did not migrate when idle");
                    }
                    Need(File.ReadAllText(UpdatePreferences.PathFor(root))==prefs,"Automatic migration changed preferences");form.Close();
                }
            }
        }
        static void AppearanceFlows(string output){
            string root=Path.Combine(output,"appearance-flow");Directory.CreateDirectory(root);File.WriteAllText(Path.Combine(root,"finish"),"done");
            var commands=new List<List<string>>();using(var form=OperationForm(root,commands)){
                new UpdatePreferences{AutomaticChecks=false,Configured=true}.Save(root);string preferences=File.ReadAllText(UpdatePreferences.PathFor(root));
                Need(Button(form,"Use improved appearance")==null&&Button(form,"Revert appearance")==null,"Routine appearance actions remain in launcher");
                Call(form,"ShowAddCharacters");Need(Button(Field<Control>(form,"extrasPage"),"Car appearance")==null,"Characters still offers routine appearance actions");Escape(form);Page(form,"homePage");
                Call(form,"ShowAppearance");Page(form,"homePage");Need(commands.Count==0,"Recovery opened without an actual appearance problem");
                Set(form,"appearanceNeedsAttention",true);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"homePage"));Click(Field<Control>(form,"homePage"),"Finish car appearance");WaitUpdate(form);Page(form,"appearancePage");
                Need(commands.Count==1&&commands[0][0]=="appearance-status","Appearance entry changed assets or ran full setup");
                var apply=Field<Button>(form,"appearanceApply");var source=Field<TextBox>(form,"appearanceSource");
                Need(apply.Text=="Retry"&&apply.Enabled&&source.Enabled&&source.Text.Contains("Synthetic"),"Material recovery source/status mapping failed");
                form.ClientSize=new Size(1536,1024);Snapshot(form,output,"appearance-recovery");
                foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);ControlsFit(Field<Control>(form,"appearancePage"),Field<Panel>(form,"pageHost"));Reach(form,apply);Snapshot(form,output,"appearance-dpi-"+dpi);}
                Dpi(form,96);form.ClientSize=new Size(960,640);ControlsFit(Field<Control>(form,"appearancePage"),Field<Panel>(form,"pageHost"));Snapshot(form,output,"appearance-minimum");
                File.WriteAllText(Path.Combine(root,"game-running"),"fixture");int before=commands.Count;apply.PerformClick();Application.DoEvents();Need(commands.Count==before&&Field<Label>(form,"notice").Text.Contains("Close the game normally"),"Appearance change ignored active game");File.Delete(Path.Combine(root,"game-running"));
                form.ClientSize=new Size(1536,1024);Click(Field<Control>(form,"appearancePage"),"Retry");WaitUpdate(form);Page(form,"homePage");Need(!Field<bool>(form,"appearanceNeedsAttention")&&!Button(Field<Control>(form,"homePage"),"Finish car appearance").Visible,"Successful recovery left an appearance prompt");Snapshot(form,output,"appearance-recovered-play");
                before=commands.Count;Call(form,"ShowAppearance");Page(form,"homePage");Need(commands.Count==before,"Successful recovery remained accessible as routine action");
                // A stale failure flag must close recovery when another attempt already succeeded.
                Set(form,"appearanceNeedsAttention",true);Call(form,"ShowAppearance");WaitUpdate(form);Page(form,"homePage");Need(!Field<bool>(form,"appearanceNeedsAttention"),"Verified active materials did not clear stale recovery");
                File.Delete(Path.Combine(root,"material-active"));Set(form,"appearanceNeedsAttention",true);Call(form,"ShowAppearance");WaitUpdate(form);
                File.WriteAllText(Path.Combine(root,"fail"),"fixture");Click(Field<Control>(form,"appearancePage"),"Retry");WaitUpdate(form);Page(form,"appearancePage");Need(apply.Enabled&&Field<Label>(form,"appearanceStatus").Text.Contains("Current appearance is unchanged"),"Unavailable source did not remain actionable on appearance page");Snapshot(form,output,"appearance-unavailable");File.Delete(Path.Combine(root,"fail"));
                Escape(form);Page(form,"homePage");Need(!Field<bool>(form,"addingCharacters"),"Recovery Back entered Characters or setup");Call(form,"ShowAppearance");WaitUpdate(form);
                File.Delete(Path.Combine(root,"finish"));Click(Field<Control>(form,"appearancePage"),"Retry");Page(form,"progressPage");Need(!Field<bool>(form,"installSteps")&&Field<bool>(form,"cancellationAvailable"),"Appearance operation used full setup steps or lost cancel");Snapshot(form,output,"appearance-progress");Call(form,"CancelOperation");WaitUpdate(form);Page(form,"appearancePage");Need(Field<Label>(form,"appearanceStatus").Text.Contains("canceled"),"Appearance cancel was not explained");
                foreach(var command in commands)Need(command[0].StartsWith("appearance-")&&!command.Contains("--sm64")&&!command.Contains("--rom")&&!command.Contains("--assets"),"Appearance action invoked full setup/import");
                Need(File.ReadAllText(UpdatePreferences.PathFor(root))==preferences,"Appearance changed launcher settings");Escape(form);Page(form,"homePage");form.Close();
            }
        }
        static void InstalledRoutes(LauncherForm form,string output){
            var navigation=Field<ConceptButton[]>(form,"navigation");Set(form,"installationReady",false);Call(form,"ShowPage",Field<FlowLayoutPanel>(form,"locationPage"));
            Need(navigation[2].Text=="SETUP","Fresh installation lost Setup tab");navigation[2].PerformClick();Application.DoEvents();Page(form,"locationPage");
            string root=Field<TextBox>(form,"install").Text,rom=Field<TextBox>(form,"rom").Text,game=Field<TextBox>(form,"game").Text;
            string prefsPath=UpdatePreferences.PathFor(root),prefs=File.Exists(prefsPath)?File.ReadAllText(prefsPath):null;
            Set(form,"lastReport",Report(true,true,"octane","link"));Call(form,"CompleteInspection",false);Page(form,"extrasPage");
            Need(navigation[2].Text=="CHARACTERS"&&navigation[2].AccessibleName=="CHARACTERS"&&Field<bool>(form,"addingCharacters"),"Verified installation did not transform Setup into Characters");
            Need(((ConceptCheckBox)Field<CheckBox[]>(form,"extraChoices")[0]).DisplayText=="Link \u00b7 Ocarina of Time","Characters text has an encoding artifact");
            Need(!Field<RadioButton>(form,"extrasNo").Visible&&!Field<RadioButton>(form,"extrasYes").Visible&&TextControl(Field<Control>(form,"extrasPage"),"Characters").Visible,"Installed Characters retained wizard controls/title");
            Need(Button(Field<Control>(form,"extrasPage"),"Car appearance")==null,"Installed Characters retained the routine appearance button");
            Dpi(form,96);form.ClientSize=new Size(1536,1024);Field<Label>(form,"notice").Text="";Snapshot(form,output,"installed-characters");
            Escape(form);Page(form,"homePage");Need(Field<Button>(form,"setupRepair").Text=="Repair installation","Installed repair action is not explicit");Snapshot(form,output,"installed-play");
            navigation[2].PerformClick();Application.DoEvents();Page(form,"extrasPage");Need(Field<bool>(form,"addingCharacters"),"Characters tab reopened base wizard");
            foreach(int dpi in new[]{96,120,144,192}){Dpi(form,dpi);ControlsFit(Field<Control>(form,"extrasPage"),Field<Panel>(form,"pageHost"));Snapshot(form,output,"installed-characters-dpi-"+dpi);}
            Dpi(form,96);form.ClientSize=new Size(960,640);ControlsFit(Field<Control>(form,"extrasPage"),Field<Panel>(form,"pageHost"));Snapshot(form,output,"installed-characters-minimum");
            Escape(form);Click(Field<Control>(form,"homePage"),"Add characters");Page(form,"extrasPage");Need(Field<bool>(form,"addingCharacters"),"Play Add characters is not standalone");Escape(form);
            Field<Button>(form,"setupRepair").PerformClick();Page(form,"failurePage");Need(Field<bool>(form,"repairOverview")&&!Field<bool>(form,"running"),"Repair entry started work automatically");Snapshot(form,output,"installed-repair");Escape(form);Page(form,"homePage");
            Need(Field<TextBox>(form,"install").Text==root&&Field<TextBox>(form,"rom").Text==rom&&Field<TextBox>(form,"game").Text==game,"Navigation lost installation/source selections");Need((File.Exists(prefsPath)?File.ReadAllText(prefsPath):null)==prefs,"Navigation rewrote existing settings");
            Set(form,"lastReport",Report(false,false,"octane"));Call(form,"CompleteInspection",true);Page(form,"locationPage");Need(navigation[2].Text=="SETUP","Incomplete installation skipped full wizard");
            Set(form,"repairOverview",false);Field<Button>(form,"retrySetup").Text="Retry / resume";Field<Button>(form,"failureBack").Text="Back / change source";
        }

        [STAThread] static int Main(string[] args) {
            if(args.Length==4&&args[0]=="--operation-fixture")return OperationFixture(args[1],args[2],args[3]);
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
                    180000
                ){TerminateProcess(pi.process,99);throw new TimeoutException("Only the test child was terminated");}}
                uint result;GetExitCodeProcess(pi.process,out result);Need(!childFocused&&InputDesktop()!=name,"Test took input desktop");
                File.WriteAllText(Path.Combine(output,"desktop-isolation.json"),new JavaScriptSerializer().Serialize(new{child_exit=result,child_foreground_seen=childFocused,input_desktop_before=input,input_desktop_after=InputDesktop(),test_desktop=name,never_called_switch_desktop=true}));
                Console.WriteLine("Separate-desktop launcher UI test exit="+result+"; no input-desktop switch; child foreground="+childFocused);return (int)result;
            } finally {if(pi.thread!=IntPtr.Zero)CloseHandle(pi.thread);if(pi.process!=IntPtr.Zero)CloseHandle(pi.process);CloseDesktop(desktop);}
        }
    }
}
