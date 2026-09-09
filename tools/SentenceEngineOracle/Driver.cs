using System;using System.Reflection;using System.Threading;using System.IO;using System.Text.Json;using TigerClaw.Core;
namespace NativeTiger.Tools;
internal static class Driver {
 sealed class Key {
 public bool settle {get;set;}=true;
 public bool reset {get;set;} public int vk {get;set;} public int scan {get;set;}
 public string action {get;set;}="down";public bool shift {get;set;} public bool ctrl {get;set;}
 public bool alt {get;set;} public bool win {get;set;} public bool caps {get;set;}
 public bool num {get;set;}=true;public int repeat {get;set;}=1;public bool extended {get;set;}
 }
 static int Main(string[] args) {try {
 if(args.Length<2 || args.Length>3)throw new ArgumentException("SentenceEngineOracle <isolated-root> <keys.jsonl>");
 var root=Path.GetFullPath(args[0]);if(!File.Exists(Path.Combine(root,".native-tiger-staging")))throw new InvalidOperationException("Staging marker required");
 using var registry=new RegistrySandbox();var state=new CoreRuntimeState(root);state.Initialize();
 InputMethodEngine engine=null;object gate=null;bool held=false;
 void Release(){if(held){Monitor.Exit(gate);held=false;}}
 void Settle(){if(engine!=null && !SpinWait.SpinUntil(()=>!engine.IsSentenceDecodePending,TimeSpan.FromSeconds(15)))throw new TimeoutException("Sentence worker did not settle");}
 try {foreach(var line in File.ReadLines(args[1])) {
 var key=JsonSerializer.Deserialize<Key>(line);
 if(engine==null || key.reset){Release();Settle();engine?.Dispose();engine=new InputMethodEngine(state,sentenceDecodeSynchronously:args.Length==2);engine.SetChinese(state.GetDefaultChinese(),out _);}
 // Hold the original reentrant engine lock across selected bursts. This only
 // controls when its unchanged worker can capture/publish, not policy results.
 if(args.Length==3 && !held){gate=typeof(InputMethodEngine).GetField("_lock",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(engine);Monitor.Enter(gate);held=true;}
 var result=engine.ProcessKey(key.vk,key.scan,key.action,key.shift,key.ctrl,key.alt,key.win,key.caps,key.num,key.repeat,key.extended);
 engine.PostProcessKey(key.vk,key.action,result,key.shift,key.ctrl,key.alt,key.win,key.caps);
 if(args.Length==3 && key.settle){Release();Settle();}
 Console.WriteLine(JsonSerializer.Serialize(new {result,snapshot=engine.GetDifferentialSnapshot(state.GetPageSize())}));
 }} finally {Release();Settle();engine?.Dispose();}return 0;
 }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
