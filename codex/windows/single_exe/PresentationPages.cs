using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed class ConceptProgress : ProgressBar {
        private readonly Timer animation=new Timer{Interval=45};private int phase;
        internal ConceptProgress(){SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint,true);animation.Tick+=delegate{if(Visible){phase=(phase+4)%80;Invalidate();}};animation.Start();}
        protected override void OnPaint(PaintEventArgs e){Graphics g=e.Graphics;g.Clear(ConceptTheme.Background);g.SmoothingMode=SmoothingMode.AntiAlias;using(var path=ConceptTheme.Round(new RectangleF(.5f,.5f,Width-1,Height-1),6))using(var pen=new Pen(ConceptTheme.Muted))g.DrawPath(pen,path);var state=g.Save();g.SetClip(new Rectangle(4,4,Math.Max(1,Width-8),Math.Max(1,Height-8)));using(var b=new SolidBrush(Color.FromArgb(0,170,210)))g.FillRectangle(b,4,4,Math.Max(1,Width*.67f),Height-8);using(var b=new SolidBrush(ConceptTheme.Cyan))for(int x=-80+phase;x<Width*.67f;x+=40)g.FillPolygon(b,new[]{new PointF(x,Height-4),new PointF(x+23,4),new PointF(x+43,4),new PointF(x+20,Height-4)});g.Restore(state);}
        protected override void Dispose(bool disposing){if(disposing)animation.Dispose();base.Dispose(disposing);}
    }
    internal sealed partial class LauncherForm {
        private void BuildSourcesConcept() {
            Button browseRom=FindButton(rom.Parent,"Browse..."),browseGame=FindButton(game.Parent,"Browse..."),next=FindButton(setupPage,"Next"),back=FindButton(setupPage,"Back"),cancel=FindButton(setupPage,"Cancel");
            var romBox=new ConceptField(rom);var gameBox=new ConceptField(game);var canvas=Canvas(setupPage);Header(canvas,"Choose your games","2 of 5  ·  Original game sources",null,Rectangle.Empty,1198,73);
            var romLabel=CopyLabel("SM64 US ROM",true);var gameLabel=CopyLabel("Rocket League folder",true);
            var romHelp=CopyLabel("Original US ROM: .z64, .v64, .n64, or a ZIP containing one ROM.",false);
            var gameHelp=CopyLabel("Windows Epic or Steam installation containing TAGame. Supported Steam profile: 25535926; identical Epic files work.",false);
            var help=CopyLabel("Leave a source blank to reuse verified assets. Car sounds are optional: unsupported sounds use the game audio fallback. Extraction tools and Python are handled automatically and verified before use. Your sources stay local.",false);
            sourceStatus.ForeColor=ConceptTheme.Muted;sourceStatus.BackColor=ConceptTheme.Surface;Primary(next);
            canvas.PaintDesign=delegate(Graphics g){Stepper(g,2);ConceptTheme.Card(g,new RectangleF(0,220,1198,193),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(0,434,1198,205),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(0,659,1198,184),ConceptTheme.Border);};
            AddLayout(canvas,delegate{PageHeight(canvas,1085);Place(romLabel,canvas,33,239,1110,43,31);Place(romHelp,canvas,33,286,1110,38,22);Place(romBox,canvas,33,335,900,58,25);Place(browseRom,canvas,952,335,212,58,25);Place(gameLabel,canvas,33,453,1110,43,31);Place(gameHelp,canvas,33,500,1110,57,22);Place(gameBox,canvas,33,568,900,58,25);Place(browseGame,canvas,952,568,212,58,25);Place(sourceStatus,canvas,33,674,1130,160,22);Place(help,canvas,5,861,1180,107,22);Place(back,canvas,0,994,226,67,27);Place(cancel,canvas,246,994,226,67,27);Place(next,canvas,909,994,289,67,27);});
        }
        private void BuildExtrasConcept() {
            Button installExtras=FindButton(extrasPage,"Install / resume"),back=FindButton(extrasPage,"Back"),cancel=FindButton(extrasPage,"Cancel");
            string[] shortNames={"Link · Ocarina of Time","Bomberman 64","Banjo-Kazooie","Spider-Man","Tony Hawk's Pro Skater"};
            var fields=new ConceptField[5];var browse=new Button[5];var formats=new Label[5];var revision=new Label[5];var badges=new Label[5];
            for(int i=0;i<5;i++){browse[i]=FindButton(extraSources[i].Parent,"Browse...");fields[i]=new ConceptField(extraSources[i]);formats[i]=CopyLabel(Commands.OptionalRomFormats(Commands.OptionalCharacters[i]),false);revision[i]=CopyLabel(extraChoices[i].Text+" · Leave source blank to reuse ready assets.",false);badges[i]=CopyLabel("NOT CHECKED",false);badges[i].TextAlign=ContentAlignment.MiddleCenter;var choice=extraChoices[i] as ConceptCheckBox;if(choice!=null)choice.DisplayText=shortNames[i];int index=i;extraChoices[i].CheckedChanged+=delegate{if(chrome!=null){ArrangeConcept();if(extraChoices[index].Checked)extraSources[index].Focus();}};}
            var canvas=Canvas(extrasPage);Header(canvas,"Optional characters","3 of 5  ·  Offline extras","05-optional-characters",new Rectangle(299,84,660,69),660,69);
            var warning=CopyLabel("VERY WIP · Some progression may not work. Switch back to Mario or Octane if stuck.",true);warning.ForeColor=Color.FromArgb(255,217,52);warning.BackColor=Color.FromArgb(32,32,14);
            extraInputs.Controls.Clear();extraInputs.AutoSize=false;extraInputs.Padding=Padding.Empty;extraInputs.Margin=Padding.Empty;extraInputs.BackColor=ConceptTheme.Background;var list=new ConceptCanvas();extraInputs.Controls.Add(list);
            var instruction=CopyLabel("Selecting a character reveals its original game source field.",false);var onlineNotice=CopyLabel("Online supports Mario and Octane only.",false);var reused=CopyLabel("Completed assets are verified and reused.",false);
            var noExtras=CopyLabel("Mario and Octane are included. You can add optional characters later from Setup.",false);
            Primary(installExtras);canvas.PaintDesign=delegate(Graphics g){Stepper(g,3);ConceptTheme.Card(g,new RectangleF(0,203,1198,49),Color.FromArgb(221,190,20));};
            list.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,0,1198,list.Height/conceptScale-1),ConceptTheme.Border);ConceptTheme.Text(g,"Choose offline extras",28,16,34,ConceptTheme.White,true);};
            extrasYes.CheckedChanged+=delegate{if(chrome!=null)ArrangeConcept();};
            AddLayout(canvas,delegate{
                float y=58;list.Zoom=conceptScale;
                for(int i=0;i<5;i++){
                    Place(extraChoices[i],list,48,y+2,890,48,24);Place(badges[i],list,980,y+3,185,39,19);
                    var errors=lastReport.ContainsKey("errors")?lastReport["errors"] as System.Collections.Generic.Dictionary<string,object>:null;badges[i].Text=readyCharacters.Contains(Commands.OptionalCharacters[i])?"READY / REUSABLE":errors!=null&&errors.ContainsKey(Commands.OptionalCharacters[i])?"NEEDS REPAIR":lastReport.ContainsKey("ready")?"NOT INSTALLED":"NOT CHECKED";
                    bool expanded=extraChoices[i].Checked;revision[i].Visible=formats[i].Visible=fields[i].Visible=browse[i].Visible=expanded;y+=51;
                    if(expanded){Place(revision[i],list,55,y,1100,35,20);Place(formats[i],list,55,y+38,1100,52,18);Place(fields[i],list,55,y+96,862,59,24);Place(browse[i],list,934,y+96,226,59,24);y+=174;}
                }
                Place(instruction,list,28,y+14,1135,35,21);Place(onlineNotice,list,69,y+64,1080,36,21);list.Size=new Size((int)(1198*conceptScale),(int)((y+116)*conceptScale));
                float listHeight=extrasYes.Checked?y+116:106,footer=336+listHeight+65;PageHeight(canvas,Math.Max(881,footer+85));
                Place(warning,canvas,60,209,1130,37,22);Place(extrasNo,canvas,29,265,550,55,24);Place(extrasYes,canvas,625,265,573,55,24);
                Place(extraInputs,canvas,0,336,1198,y+116,22);extraInputs.Visible=extrasYes.Checked;Place(noExtras,canvas,31,355,1140,80,27);noExtras.Visible=!extrasYes.Checked;
                Place(reused,canvas,44,footer-48,1145,34,21);Place(back,canvas,0,footer,195,62,27);Place(cancel,canvas,216,footer,194,62,27);Place(installExtras,canvas,909,footer,289,62,27);
            });
        }
        private void BuildOperationConcepts() {
            var progressCanvas=Canvas(progressPage);Header(progressCanvas,"Getting ready","4 of 5  ·  Installing","06-operation-states",new Rectangle(190,63,324,43),551,73);
            var completed=CopyLabel("Completed assets stay available if you cancel or retry.",false);progressText.ForeColor=ConceptTheme.White;progressText.BackColor=ConceptTheme.Surface;
            progressCanvas.PaintDesign=delegate(Graphics g){Stepper(g,4);ConceptTheme.Card(g,new RectangleF(0,220,1198,481),ConceptTheme.Border);g.DrawImage(ConceptTheme.Art("06-operation-states"),new Rectangle(66,320,180,180),new Rectangle(215,256,91,91),GraphicsUnit.Pixel);};
            AddLayout(progressCanvas,delegate{PageHeight(progressCanvas,781);Place(progressText,progressCanvas,310,298,825,150,43);Place(progress,progressCanvas,310,466,825,52,24);Place(completed,progressCanvas,146,593,1000,55,25);Place(cancelOperation,chrome,302,ClientSize.Height/conceptScale-127,425,65,27);});
            Button readyPlay=FindButton(readyPage,"Play Offline"),open=FindButton(readyPage,"Open launcher");var readyCanvas=Canvas(readyPage);
            Header(readyCanvas,"Ready to play","5 of 5  ·  Setup complete","06-operation-states",new Rectangle(958,63,320,47),497,73);
            float readyExtra=0;var updateLabel=CopyLabel("Choose how to update",true);var disclosure=CopyLabel("Automatic: check, download, verify and apply at startup.\nManual: no startup checks.\nAssets, controls and saves are preserved.",false);readyStatus.ForeColor=ConceptTheme.White;readyStatus.BackColor=ConceptTheme.Surface;Primary(readyPlay);
            readyCanvas.PaintDesign=delegate(Graphics g){Stepper(g,5);ConceptTheme.Card(g,new RectangleF(0,220,1198,556+readyExtra),ConceptTheme.Border);using(var p=new Pen(Color.FromArgb(150,241,82),8))g.DrawEllipse(p,73,266,125,125);ConceptTheme.Icon(g,"check",new RectangleF(98,300,76,54),Color.FromArgb(150,241,82));using(var p=new Pen(ConceptTheme.Border)){g.DrawLine(p,43,427+readyExtra,1152,427+readyExtra);g.DrawLine(p,43,570+readyExtra,1152,570+readyExtra);}};
            AddLayout(readyCanvas,delegate{readyExtra=CopyHeight(readyStatus.Text,880,28,168)-168;PageHeight(readyCanvas,881+readyExtra);Place(readyStatus,readyCanvas,270,247,880,168+readyExtra,28);Place(readyMenu,readyCanvas,45,443+readyExtra,1050,48,24);Place(readyDesktop,readyCanvas,45,501+readyExtra,1050,48,24);Place(updateLabel,readyCanvas,43,586+readyExtra,680,42,28);Place(readyAuto,readyCanvas,43,635+readyExtra,659,46,23);Place(readyManual,readyCanvas,43,698+readyExtra,659,46,23);Place(disclosure,readyCanvas,752,603+readyExtra,400,151,21);Place(open,readyCanvas,0,810+readyExtra,547,67,27);Place(readyPlay,readyCanvas,656,810+readyExtra,542,67,27);});
            Button retryUpdate=FindButton(updatePage,"Retry update check"),updateSettings=FindButton(updatePage,"Settings");var updateCanvas=Canvas(updatePage);
            Header(updateCanvas,"Launcher update","Your installed version and data are protected.","06-operation-states",new Rectangle(193,620,350,39),655,73);Primary(installUpdate);updateMessage.ForeColor=ConceptTheme.Muted;updateMessage.BackColor=ConceptTheme.Surface;
            float updateExtra=0;var keepData=CopyLabel("Assets, saves and controller settings stay in the data folder.\nThe previous launcher is kept for rollback.",false);var gameWarning=CopyLabel("Close any running game before updating.",true);gameWarning.ForeColor=Color.FromArgb(255,214,67);gameWarning.BackColor=Color.FromArgb(33,30,12);
            updateCanvas.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,154,1198,665+updateExtra),ConceptTheme.Border);g.DrawImage(ConceptTheme.Art("06-operation-states"),new Rectangle(69,210,169,169),new Rectangle(235,719,91,82),GraphicsUnit.Pixel);ConceptTheme.Card(g,new RectangleF(40,485+updateExtra,1118,77),Color.FromArgb(197,167,45));};
            AddLayout(updateCanvas,delegate{float messageHeight=CopyHeight(updateMessage.Text,838,26,148);updateExtra=Math.Max(0,messageHeight-148);PageHeight(updateCanvas,881+updateExtra);Place(updateMessage,updateCanvas,312,192,838,messageHeight,26);Place(keepData,updateCanvas,312,355+updateExtra,838,106,24);Place(gameWarning,updateCanvas,160,502+updateExtra,970,44,25);Place(installUpdate,updateCanvas,40,595+updateExtra,1118,86,30);Place(retryUpdate,updateCanvas,40,715+updateExtra,365,69,23);Place(useInstalled,updateCanvas,424,715+updateExtra,391,69,23);Place(updateSettings,updateCanvas,836,715+updateExtra,322,69,23);});
            Button retry=FindButton(failurePage,"Retry / resume"),repair=FindButton(failurePage,"Repair program files"),close=FindButton(failurePage,"Close");var failureCanvas=Canvas(failurePage);
            Header(failureCanvas,"Let's finish setup","Your completed work is safe.","06-operation-states",new Rectangle(960,620,393,41),700,73);Primary(retry);failureText.ForeColor=Color.FromArgb(255,214,67);failureText.BackColor=Color.FromArgb(31,29,15);
            float failureExtra=0;var optionalNote=CopyLabel("For optional-character failures only",false);
            failureCanvas.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,186,1198,205+failureExtra),Color.FromArgb(194,161,24));ConceptTheme.Icon(g,"info",new RectangleF(35,243,71,71),Color.FromArgb(255,214,67));};
            AddLayout(failureCanvas,delegate{failureExtra=CopyHeight(failureText.Text,1010,27,164)-164;PageHeight(failureCanvas,881+failureExtra);Place(failureText,failureCanvas,147,207,1010,164+failureExtra,27);Place(retry,failureCanvas,0,420+failureExtra,1198,87,30);Place(failureBack,failureCanvas,0,535+failureExtra,386,72,25);Place(repair,failureCanvas,407,535+failureExtra,459,72,25);Place(close,failureCanvas,886,535+failureExtra,312,72,25);Place(optionalNote,failureCanvas,4,656+failureExtra,1185,43,21);optionalNote.Visible=skipOptional.Visible;Place(skipOptional,failureCanvas,0,710+failureExtra,1198,78,25);});
        }
    }
}
