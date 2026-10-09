using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed class ConceptProgress : ProgressBar {
        private readonly Timer animation=new Timer{Interval=35};private readonly System.Diagnostics.Stopwatch clock=System.Diagnostics.Stopwatch.StartNew();
        internal ConceptProgress(){SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint,true);animation.Tick+=delegate{if(Visible)Invalidate();};animation.Start();}
        protected override void OnPaint(PaintEventArgs e){Graphics g=e.Graphics;g.Clear(ConceptTheme.Background);g.SmoothingMode=SmoothingMode.AntiAlias;using(var path=ConceptTheme.Round(new RectangleF(.5f,.5f,Width-1,Height-1),6))using(var pen=new Pen(ConceptTheme.Muted))g.DrawPath(pen,path);float width=Math.Max(1,Width-8),height=Math.Max(1,Height-8);var state=g.Save();g.SetClip(new RectangleF(4,4,width,height));using(var b=new SolidBrush(ConceptTheme.Cyan)){if(Style==ProgressBarStyle.Marquee){float segment=width*.18f;float offset=(float)(clock.ElapsedMilliseconds%1800)/1800f*(width+segment)-segment;g.FillRectangle(b,4+offset,4,segment,height);}else{float fraction=Maximum>Minimum?(float)(Value-Minimum)/(Maximum-Minimum):0;g.FillRectangle(b,4,4,width*fraction,height);}}g.Restore(state);}
        protected override void Dispose(bool disposing){if(disposing)animation.Dispose();base.Dispose(disposing);}
    }
    internal sealed partial class LauncherForm {
        private void BuildSourcesConcept() {
            Button browseRom=FindButton(rom.Parent,"Browse..."),browseGame=FindButton(game.Parent,"Browse..."),next=FindButton(setupPage,"Next"),back=FindButton(setupPage,"Back"),cancel=FindButton(setupPage,"Cancel");
            var romBox=new ConceptField(rom);var gameBox=new ConceptField(game);var canvas=Canvas(setupPage);Header(canvas,"Choose your games","2 of 5  ·  Original game sources",null,Rectangle.Empty,1198,73);
            var romLabel=CopyLabel("SM64 US ROM",true);var gameLabel=CopyLabel("Rocket League folder",true);
            romFeedback.ForeColor=gameFeedback.ForeColor=ConceptTheme.Muted;romFeedback.BackColor=gameFeedback.BackColor=ConceptTheme.Surface;Primary(next);
            canvas.PaintDesign=delegate(Graphics g){Stepper(g,2);ConceptTheme.Card(g,new RectangleF(0,220,1198,203),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(0,443,1198,203),ConceptTheme.Border);};
            AddLayout(canvas,delegate{PageHeight(canvas,790);Place(romLabel,canvas,33,239,1110,43,31);Place(romBox,canvas,33,291,900,58,25);Place(browseRom,canvas,952,291,212,58,25);Place(romFeedback,canvas,33,363,1130,52,23);Place(gameLabel,canvas,33,462,1110,43,31);Place(gameBox,canvas,33,514,900,58,25);Place(browseGame,canvas,952,514,212,58,25);Place(gameFeedback,canvas,33,586,1130,52,23);Place(back,canvas,0,684,226,67,27);Place(cancel,canvas,246,684,226,67,27);Place(next,canvas,909,684,289,67,27);});
        }
        private void BuildExtrasConcept() {
            Button installExtras=FindButton(extrasPage,"Install / resume"),back=FindButton(extrasPage,"Back"),cancel=FindButton(extrasPage,"Cancel");
            string[] shortNames={"Link · Ocarina of Time","Bomberman 64","Banjo-Kazooie","Spider-Man","Tony Hawk's Pro Skater"};
            var fields=new ConceptField[5];var browse=new Button[5];var formats=new Label[5];
            for(int i=0;i<5;i++){
                browse[i]=FindButton(extraSources[i].Parent,"Browse...");fields[i]=new ConceptField(extraSources[i]);formats[i]=CopyLabel(Commands.OptionalRomFormats(Commands.OptionalCharacters[i]),false);
                var choice=extraChoices[i] as ConceptCheckBox;if(choice!=null)choice.DisplayText=shortNames[i];int index=i;
                extraChoices[i].CheckedChanged+=delegate{if(chrome!=null){ArrangeConcept();if(extraChoices[index].Checked)extraSources[index].Focus();}};
            }
            var canvas=Canvas(extrasPage);Header(canvas,"Optional characters","3 of 5  ·  Offline extras","05-optional-characters",new Rectangle(299,84,660,69),660,69);
            var wizardHeading=canvas.Controls[0];var charactersHeading=CopyLabel("Characters",true);
            var warning=CopyLabel("VERY WIP · Some progression may not work. Switch back to Mario or Octane if stuck.",true);warning.ForeColor=Color.FromArgb(255,217,52);warning.BackColor=Color.FromArgb(32,32,14);
            extraInputs.Controls.Clear();extraInputs.AutoSize=false;extraInputs.Padding=Padding.Empty;extraInputs.Margin=Padding.Empty;extraInputs.BackColor=ConceptTheme.Background;var list=new ConceptCanvas();extraInputs.Controls.Add(list);
            var onlineNotice=CopyLabel("Optional characters are for offline play.",false);
            var noExtras=CopyLabel("Mario and Octane are included. You can add optional characters later from Play.",false);
            Primary(installExtras);canvas.PaintDesign=delegate(Graphics g){if(!addingCharacters)Stepper(g,3);ConceptTheme.Card(g,new RectangleF(0,addingCharacters?124:203,1198,49),Color.FromArgb(221,190,20));};
            list.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,0,1198,459),ConceptTheme.Border);};
            extrasYes.CheckedChanged+=delegate{if(chrome!=null)ArrangeConcept();};
            AddLayout(canvas,delegate{
                list.Zoom=conceptScale;
                for(int i=0;i<5;i++){
                    float y=12+i*89;Place(extraChoices[i],list,24,y,405,52,23);
                    bool expanded=extraChoices[i].Checked;formats[i].Visible=fields[i].Visible=browse[i].Visible=expanded;
                    Place(fields[i],list,442,y,588,50,22);Place(browse[i],list,1047,y,126,50,21);Place(formats[i],list,443,y+53,730,31,17);
                }
                list.Size=new Size((int)(1198*conceptScale),(int)(460*conceptScale));
                float footer=addingCharacters?700:extrasYes.Checked?825:550;PageHeight(canvas,addingCharacters?790:895);
                wizardHeading.Visible=!addingCharacters;Place(charactersHeading,canvas,0,0,1198,73,60);charactersHeading.Visible=addingCharacters;
                canvas.Controls[1].Text=addingCharacters?"Mario and Octane are ready. Manage your offline extras.":"3 of 5  -  Offline extras";
                Place(warning,canvas,60,addingCharacters?130:209,1130,37,22);Place(extrasNo,canvas,29,265,550,55,24);Place(extrasYes,canvas,625,265,573,55,24);extrasNo.Visible=extrasYes.Visible=!addingCharacters;
                Place(extraInputs,canvas,0,addingCharacters?199:336,1198,460,22);extraInputs.Visible=extrasYes.Checked;Place(noExtras,canvas,31,355,1140,80,27);noExtras.Visible=!extrasYes.Checked;
                Place(onlineNotice,canvas,31,addingCharacters?668:extrasYes.Checked?798:footer-34,1140,24,19);Place(back,canvas,0,footer,195,62,27);Place(cancel,canvas,216,footer,194,62,27);Place(installExtras,canvas,909,footer,289,62,27);
            });
        }

        private void BuildOperationConcepts() {
            var progressCanvas=Canvas(progressPage);Header(progressCanvas,"Getting ready","4 of 5  ·  Installing","06-operation-states",new Rectangle(190,63,324,43),551,73);
            var completed=CopyLabel("Completed assets stay available if you cancel or retry.",false);progressText.ForeColor=ConceptTheme.White;progressText.BackColor=ConceptTheme.Surface;
            progressCanvas.PaintDesign=delegate(Graphics g){if(installSteps)Stepper(g,4);int offset=installSteps?0:-66;ConceptTheme.Card(g,new RectangleF(0,220+offset,1198,481),ConceptTheme.Border);g.DrawImage(ConceptTheme.Art("06-operation-states"),new Rectangle(66,320+offset,180,180),new Rectangle(215,256,91,91),GraphicsUnit.Pixel);};
            AddLayout(progressCanvas,delegate{float offset=installSteps?0:-66;progressCanvas.Controls[1].Text=progressSubtitle;completed.Text=progressSubtitle=="Updating launcher"?"Your installed version and game data stay protected.":"Completed assets stay available if you cancel or retry.";PageHeight(progressCanvas,781+offset);Place(progressText,progressCanvas,310,298+offset,825,150,43);Place(progress,progressCanvas,310,466+offset,825,52,24);Place(completed,progressCanvas,146,593+offset,1000,55,25);Place(cancelOperation,chrome,302,ClientSize.Height/conceptScale-127,425,65,27);});
            Button readyPlay=FindButton(readyPage,"Play Offline"),open=FindButton(readyPage,"Open launcher");var readyCanvas=Canvas(readyPage);
            Header(readyCanvas,"Ready to play","5 of 5  ·  Setup complete","06-operation-states",new Rectangle(958,63,320,47),497,73);
            float readyExtra=0;var updateLabel=CopyLabel("Update notifications",true);var disclosure=CopyLabel("Notifications: choose Update now or Later.\nManual: no startup checks.\nAssets, controls and saves are preserved.",false);readyStatus.ForeColor=ConceptTheme.White;readyStatus.BackColor=ConceptTheme.Surface;Primary(readyPlay);
            readyCanvas.PaintDesign=delegate(Graphics g){Stepper(g,5);ConceptTheme.Card(g,new RectangleF(0,220,1198,556+readyExtra),ConceptTheme.Border);using(var p=new Pen(Color.FromArgb(150,241,82),8))g.DrawEllipse(p,73,266,125,125);ConceptTheme.Icon(g,"check",new RectangleF(98,300,76,54),Color.FromArgb(150,241,82));using(var p=new Pen(ConceptTheme.Border)){g.DrawLine(p,43,427+readyExtra,1152,427+readyExtra);g.DrawLine(p,43,570+readyExtra,1152,570+readyExtra);}};
            AddLayout(readyCanvas,delegate{readyExtra=CopyHeight(readyStatus.Text,880,28,168)-168;PageHeight(readyCanvas,881+readyExtra);Place(readyStatus,readyCanvas,270,247,880,168+readyExtra,28);Place(readyMenu,readyCanvas,45,443+readyExtra,1050,48,24);Place(readyDesktop,readyCanvas,45,501+readyExtra,1050,48,24);Place(updateLabel,readyCanvas,43,586+readyExtra,680,42,28);Place(readyAuto,readyCanvas,43,635+readyExtra,659,46,23);Place(readyManual,readyCanvas,43,698+readyExtra,659,46,23);Place(disclosure,readyCanvas,752,603+readyExtra,400,151,21);Place(open,readyCanvas,0,810+readyExtra,547,67,27);Place(readyPlay,readyCanvas,656,810+readyExtra,542,67,27);});
            var updateCanvas=Canvas(updatePage);
            Header(updateCanvas,"Launcher update","Your installed version and data are protected.","06-operation-states",new Rectangle(193,620,350,39),655,73);Primary(installUpdate);updateMessage.ForeColor=ConceptTheme.Muted;updateMessage.BackColor=ConceptTheme.Surface;
            float updateExtra=0;var keepData=CopyLabel("Assets, saves and controller settings stay in the data folder.\nThe previous launcher is kept for rollback.",false);var gameWarning=CopyLabel("Close any running game before updating.",true);gameWarning.ForeColor=Color.FromArgb(255,214,67);gameWarning.BackColor=Color.FromArgb(33,30,12);
            updateCanvas.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,154,1198,665+updateExtra),ConceptTheme.Border);g.DrawImage(ConceptTheme.Art("06-operation-states"),new Rectangle(69,210,169,169),new Rectangle(235,719,91,82),GraphicsUnit.Pixel);ConceptTheme.Card(g,new RectangleF(40,485+updateExtra,1118,77),Color.FromArgb(197,167,45));};
            AddLayout(updateCanvas,delegate{float messageHeight=CopyHeight(updateMessage.Text,838,26,148);updateExtra=Math.Max(0,messageHeight-148);PageHeight(updateCanvas,881+updateExtra);Place(updateMessage,updateCanvas,312,192,838,messageHeight,26);Place(keepData,updateCanvas,312,355+updateExtra,838,106,24);Place(gameWarning,updateCanvas,160,502+updateExtra,970,44,25);Place(installUpdate,updateCanvas,40,595+updateExtra,1118,86,30);Place(retryUpdate,updateCanvas,40,715+updateExtra,550,69,23);Place(useInstalled,updateCanvas,610,715+updateExtra,550,69,23);retryUpdate.Visible=useInstalled.Visible=updateError;});
            Button retry=FindButton(failurePage,"Retry / resume"),repair=FindButton(failurePage,"Repair program files"),close=FindButton(failurePage,"Close");var failureCanvas=Canvas(failurePage);
            Header(failureCanvas,"Let's finish setup","Your completed work is safe.","06-operation-states",new Rectangle(960,620,393,41),700,73);Primary(retry);failureText.ForeColor=Color.FromArgb(255,214,67);failureText.BackColor=Color.FromArgb(31,29,15);
            var recoveryHeading=failureCanvas.Controls[0];var repairHeading=CopyLabel("Repair installation",true);
            float failureExtra=0;var optionalNote=CopyLabel("For optional-character failures only",false);
            failureCanvas.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,186,1198,205+failureExtra),Color.FromArgb(194,161,24));ConceptTheme.Icon(g,"info",new RectangleF(35,243,71,71),Color.FromArgb(255,214,67));};
            AddLayout(failureCanvas,delegate{recoveryHeading.Visible=!repairOverview;Place(repairHeading,failureCanvas,0,0,1198,73,60);repairHeading.Visible=repairOverview;failureExtra=CopyHeight(failureText.Text,1010,27,164)-164;PageHeight(failureCanvas,881+failureExtra);Place(failureText,failureCanvas,147,207,1010,164+failureExtra,27);Place(retry,failureCanvas,0,420+failureExtra,1198,87,30);Place(failureBack,failureCanvas,0,535+failureExtra,386,72,25);Place(repair,failureCanvas,407,535+failureExtra,459,72,25);Place(close,failureCanvas,886,535+failureExtra,312,72,25);Place(optionalNote,failureCanvas,4,656+failureExtra,1185,43,21);optionalNote.Visible=skipOptional.Visible;Place(skipOptional,failureCanvas,0,710+failureExtra,1198,78,25);});
        }
    }
}
