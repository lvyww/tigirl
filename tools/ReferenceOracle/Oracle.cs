using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.Json;
using TigerClaw.Core;

namespace NativeTiger.Tools;

// This is an offline reference/export utility, never loaded by the TSF DLL.
internal static class Oracle
{
    static T Field<T>(CoreRuntimeState state, string name) =>
        (T)typeof(CoreRuntimeState).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(state);

    static int Main(string[] args)
    {
        try
        {
            if (args.Length != 3 || (args[0] != "export" && args[0] != "trace" && args[0] != "changes" && args[0] != "selection" && args[0] != "settings" && args[0] != "presentation" && args[0] != "dynamic" && args[0] != "currency" && args[0] != "addword" && args[0] != "mixed" && args[0] != "tracehistory" && args[0] != "timer" && args[0] != "switchcomposition" && args[0] != "parserows" && args[0] != "mergerows" && args[0] != "constructrows" && args[0] != "auxiliaryrows" && args[0] != "decodebytes" && args[0] != "orderfiles"))
                throw new ArgumentException("Usage: ReferenceOracle <export|trace|changes> <isolated-runtime-root> <output.tcd|input.jsonl>");
            string root = Path.GetFullPath(args[1]);
            // Initialization may rewrite config/recent-table metadata. Explicitly require a
            // staging marker so the oracle can never target the user's live installation.
            if (!File.Exists(Path.Combine(root, ".native-tiger-staging")))
                throw new InvalidOperationException("An isolated runtime with .native-tiger-staging is required.");
            using var registrySandbox = new RegistrySandbox();
            var state = new CoreRuntimeState(root);
            state.Initialize();
            if (args[0] == "export") Export(state, args[2]);
            else if (args[0] == "trace") Trace(state, args[2]);
            else if(args[0] == "tracehistory") Trace(state,args[2],true);
            else if (args[0] == "changes") Changes(state, args[2]);
            else if (args[0] == "selection") Selection(args[2]);
            else if (args[0] == "settings") Settings(state, root, args[2]);
            else if (args[0] == "presentation") Presentation(args[2]);
            else if(args[0] == "currency") Currency(args[2]);
            else if(args[0] == "timer") TimerSpec(args[2]);
            else if(args[0] == "switchcomposition") SwitchComposition(state,args[2]);
            else if(args[0] == "parserows") ParseRows(root,args[2]);
            else if(args[0] == "mergerows") MergeRows(state,root,args[2]);
            else if(args[0] == "constructrows") MergeRows(state,root,args[2],true);
            else if(args[0] == "auxiliaryrows") AuxiliaryRows(state,root,args[2]);
            else if(args[0] == "decodebytes") DecodeBytes(root,args[2]);
            else if(args[0] == "orderfiles") OrderFiles(root,args[2]);
            else if(args[0] == "addword") AddWord(state,args[2]);
            else if(args[0] == "mixed") Mixed(args[2]);
            else Dynamic(state,args[2]);
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }

    static void OrderFiles(string root,string input)
    {
        foreach(string line in File.ReadLines(input)) {
            using var doc=JsonDocument.Parse(line);var row=doc.RootElement;
            System.Globalization.CultureInfo.CurrentCulture=new System.Globalization.CultureInfo(row.GetProperty("culture").GetString());
            string directory=Path.Combine(root,row.GetProperty("directory").GetString());
            Console.WriteLine(JsonSerializer.Serialize(CoreRuntimeState.GetOrderedLexiconFiles(directory)
                .Select(p=>string.Concat(Path.GetFileName(p).Select(c=>((int)c).ToString("x4"))))));
        }
    }
    static void DecodeBytes(string root,string input)
    {
        var detect=typeof(CoreRuntimeState).GetMethod("DetectTextEncoding",BindingFlags.Static|BindingFlags.NonPublic);
        string fixture=Path.Combine(root,"decode-fixture.bin");
        foreach(string line in File.ReadLines(input)) {
            File.WriteAllBytes(fixture,Convert.FromHexString(line));
            var encoding=(Encoding)detect.Invoke(null,new object[]{fixture});
            // Same StreamReader encoding/preamble defaults as the original ReadLines.
            string decoded=File.ReadAllText(fixture,encoding);
            Console.WriteLine(string.Concat(decoded.Select(c=>((int)c).ToString("x4"))));
        }
    }
    sealed record Entry(string Key, uint Flags, string[] Values);
    static void Mixed(string path)
    {
        int calls=0;
        var decoder=new FixedLengthMixedInputDecoder(code => {
            ++calls;
            string key=code.ToLowerInvariant();
            return key.StartsWith("z") ? new List<string>() : new List<string>{"显示\x1e"+key+":"+calls};
        }, packed => packed.Substring(packed.IndexOf('\x1e')+1));
        foreach(var line in File.ReadLines(path)) {
            using var document=JsonDocument.Parse(line); var row=document.RootElement;
            if(row.GetProperty("clear").GetBoolean()) decoder.ClearCache();
            var preferred=new Dictionary<int,string>();
            foreach(var item in row.GetProperty("preferred").EnumerateObject()) preferred.Add(int.Parse(item.Name),item.Value.GetString());
            var result=decoder.Decode(new MixedInputDecodeRequest {
                RawCode=row.GetProperty("raw").GetString(), MaxCodeLength=row.GetProperty("maximum").GetInt32(),
                LexiconVersion=row.GetProperty("version").GetInt32(), PreferredCandidateTextByStart=preferred});
            Console.WriteLine(JsonSerializer.Serialize(new {raw=result.RawCode,prefix=result.ResolvedPrefixText,active=result.ActiveCode,surface=result.SurfaceText,
                segments=result.Segments.Select(s=>new {code=s.Code,candidate=s.CandidateText}),calls,
                chinese=MixedInputCommitComposer.ComposeChinese(result,"尾","。"),english=MixedInputCommitComposer.ComposeRaw(result)}));
        }
    }
    static void AddWord(CoreRuntimeState state,string path)
    {
        var parse=typeof(CoreRuntimeState).GetMethod("ParseLexiconEntryToken",BindingFlags.Static|BindingFlags.NonPublic);
        foreach(var line in File.ReadLines(path)) {
            var text=JsonSerializer.Deserialize<string>(line);
            var elements=new List<string>();
            var e=System.Globalization.StringInfo.GetTextElementEnumerator(text);
            while(e.MoveNext()) elements.Add(e.GetTextElement());
            Console.WriteLine(JsonSerializer.Serialize(new {text,code=state.ConstructCi(text),packed=(string)parse.Invoke(null,new object[]{text}),normalized=text.Trim().ToLowerInvariant(),elements}));
        }
    }
    static void AuxiliaryRows(CoreRuntimeState state,string root,string path)
    {
        var pyMethod=typeof(CoreRuntimeState).GetMethod("LoadPinyinLexicon",BindingFlags.Instance|BindingFlags.NonPublic);
        var commentMethod=typeof(CoreRuntimeState).GetMethod("LoadCommentMap",BindingFlags.Static|BindingFlags.NonPublic);
        var splitMethod=typeof(CoreRuntimeState).GetMethod("LoadSplitMap",BindingFlags.Static|BindingFlags.NonPublic);
        var pyRoot=Path.Combine(root,"拼音反查码表");Directory.CreateDirectory(pyRoot);
        var directory=Path.Combine(root,"native-aux-fixture");Directory.CreateDirectory(directory);
        string Hex(string text)=>string.Concat(text.Select(c=>((int)c).ToString("x4")));
        foreach(var line in File.ReadLines(path)) {
            using var document=JsonDocument.Parse(line);var row=document.RootElement;
            File.WriteAllText(Path.Combine(pyRoot,"rows.txt"),row.GetProperty("pinyin").GetString(),new UTF8Encoding(false));
            File.WriteAllText(Path.Combine(directory,"rows.注释"),row.GetProperty("comments").GetString(),new UTF8Encoding(false));
            File.WriteAllText(Path.Combine(directory,"rows.拆分"),row.GetProperty("splits").GetString(),new UTF8Encoding(false));
            var pinyin=(Dictionary<string,List<string>>)pyMethod.Invoke(state,null);
            object Lookup(MethodInfo method)=>((Dictionary<string,string>)method.Invoke(null,new object[]{directory})).OrderBy(x=>x.Key,StringComparer.Ordinal).Select(x=>new[]{Hex(x.Key),Hex(x.Value)});
            Console.WriteLine(JsonSerializer.Serialize(new {pinyin=pinyin.Select(x=>new object[]{Hex(x.Key),x.Value.Select(Hex)}),comments=Lookup(commentMethod),splits=Lookup(splitMethod)}));
        }
    }
    static void MergeRows(CoreRuntimeState state,string root,string path,bool construct=false)
    {
        var method=typeof(CoreRuntimeState).GetMethod("BuildLexiconSnapshot",BindingFlags.Instance|BindingFlags.NonPublic);
        var dir=Path.Combine(root,"native-merge-fixture");Directory.CreateDirectory(dir);
        foreach(var line in File.ReadLines(path)) {
            using var document=JsonDocument.Parse(line);var row=document.RootElement;
            bool directoryInput=row.TryGetProperty("directory",out var directoryValue);
            var snapshotDirectory=directoryInput?Path.Combine(root,directoryValue.GetString()):dir;
            if(directoryInput) System.Globalization.CultureInfo.CurrentCulture=new System.Globalization.CultureInfo(row.GetProperty("culture").GetString());
            else File.WriteAllText(Path.Combine(dir,"rows.txt"),row.GetProperty("text").GetString(),new UTF8Encoding(false));
            if(construct && !directoryInput) {
                var constructionPath=Path.Combine(dir,"构词.txt");
                if(row.GetProperty("construction").ValueKind==JsonValueKind.Null) File.Delete(constructionPath);
                else File.WriteAllText(constructionPath,row.GetProperty("construction").GetString(),new UTF8Encoding(false));
                var adjustmentPath=Path.Combine(dir,"用户调整.txt");
                if(row.TryGetProperty("adjustments",out var adjustments)) File.WriteAllText(adjustmentPath,adjustments.GetString(),new UTF8Encoding(false));
                else File.Delete(adjustmentPath);
            }
            var snapshot=method.Invoke(state,new object[]{snapshotDirectory});
            var map=(Dictionary<string,List<string>>)snapshot.GetType().GetField("Lexicon",BindingFlags.Public|BindingFlags.Instance).GetValue(snapshot);
            if(construct) {
                string Hex(string value) => string.Concat(value.Select(c=>((int)c).ToString("x4")));
                object Lookup(string field) => ((Dictionary<string,string>)snapshot.GetType().GetField(field,BindingFlags.Public|BindingFlags.Instance).GetValue(snapshot))
                    .OrderBy(x=>x.Key,StringComparer.Ordinal).Select(x=>new[]{Hex(x.Key),Hex(x.Value)});
                T Value<T>(string field) => (T)snapshot.GetType().GetField(field,BindingFlags.Public|BindingFlags.Instance).GetValue(snapshot);
                var unique=Value<HashSet<string>>("Unique");var prefixes=Value<HashSet<string>>("NonTerminal");var auto=Value<HashSet<string>>("AutoShortSymbol");
                var indexed=map.Keys.Concat(prefixes).Distinct(StringComparer.Ordinal).OrderBy(x=>x,StringComparer.Ordinal)
                    .Select(code=>new object[]{Hex(code),(unique.Contains(code)?1:0)|(prefixes.Contains(code)?2:0)|(auto.Contains(code)?4:0)|(map.ContainsKey(code)?8:0),
                        map.TryGetValue(code,out var candidates)?candidates.Select(Hex):Array.Empty<string>()});
                int quick=(Value<bool>("ShortSymbolSemicolon")?1:0)|(Value<bool>("ShortSymbolSlash")?2:0)|(Value<bool>("ShortSymbolLBracket")?4:0)|(Value<bool>("ShortSymbolZ")?8:0);
                if(row.TryGetProperty("all",out var all) && all.GetBoolean()) {
                    var pinyin=Value<Dictionary<string,List<string>>>("PinyinLexicon");
                    Console.WriteLine(JsonSerializer.Serialize(new {main=map.Select(x=>new object[]{Hex(x.Key),x.Value.Select(Hex)}),construct=Lookup("ConstructCodeMap"),full=Lookup("FullCodeMap"),indexed,quick,
                        comments=Lookup("CommentMap"),splits=Lookup("SplitMap"),pinyin=pinyin.Select(x=>new object[]{Hex(x.Key),x.Value.Select(Hex)})}));
                } else Console.WriteLine(JsonSerializer.Serialize(new {main=map.Select(x=>new object[]{Hex(x.Key),x.Value.Select(Hex)}),construct=Lookup("ConstructCodeMap"),full=Lookup("FullCodeMap"),indexed,quick}));
            } else Console.WriteLine(JsonSerializer.Serialize(map.Select(x=>new object[]{x.Key,x.Value})));
        }
    }
    static void ParseRows(string root,string path)
    {
        var method=typeof(CoreRuntimeState).GetMethod("ParseMbFile",BindingFlags.Static|BindingFlags.NonPublic,null,
            new[]{typeof(string),typeof(List<(string,string,int)>),typeof(List<(string,int)>)},null);
        foreach(var line in File.ReadLines(path)) {
            using var document=JsonDocument.Parse(line);var row=document.RootElement;
            var file=Path.Combine(root,"native-row-fixture"+(row.GetProperty("yaml").GetBoolean()?".dict.yaml":".txt"));
            File.WriteAllText(file,row.GetProperty("text").GetString(),new UTF8Encoding(false));
            var coded=new List<(string code,string text,int freq)>();var uncoded=new List<(string text,int freq)>();
            method.Invoke(null,new object[]{file,coded,uncoded});
            Console.WriteLine(JsonSerializer.Serialize(new {coded=coded.Select(x=>new object[]{x.code,x.text,x.freq}),uncoded=uncoded.Select(x=>new object[]{x.text,x.freq})}));
        }
    }
    static void SwitchComposition(CoreRuntimeState state,string path)
    {
        void Set(string key,string value) {
            if(!state.TrySetConfigValue(key,value,out _,out string error)) throw new InvalidOperationException(error);
        }
        foreach(var line in File.ReadLines(path)) {
            using var document=JsonDocument.Parse(line);var row=document.RootElement;
            Set("最大码长","16");Set("中英文不限长混合输入",row.GetProperty("beforeMixed").GetBoolean()?"是":"否");
            using var engine=new InputMethodEngine(state);
            foreach(char c in row.GetProperty("text").GetString()) {
                int vk=c=='`'?192:c=='='?187:c==' '?32:char.ToUpperInvariant(c);
                bool shift=char.IsUpper(c);
                foreach(string action in new[]{"down","up"}) {
                    var result=engine.ProcessKey(vk,0,action,shift,false,false,false,false,true,1,false);
                    engine.PostProcessKey(vk,action,result,shift,false,false,false,false);
                }
            }
            Set("最大码长",row.GetProperty("maximum").GetInt32().ToString());
            Set("中英文不限长混合输入",row.GetProperty("afterMixed").GetBoolean()?"是":"否");
            engine.RefreshCompositionAfterSchemaSwitch();
            var snapshot=engine.GetDifferentialSnapshot(state.GetPageSize());var ui=snapshot.Ui;
            var commit=engine.ProcessKey(32,0,"down",false,false,false,false,false,true,1,false);
            engine.PostProcessKey(32,"down",commit,false,false,false,false,false);
            Console.WriteLine(JsonSerializer.Serialize(new {raw=snapshot.RawInput,surface=ui.InputCode,mode=ui.CompositionState,page=snapshot.CandidatePageIndex,candidates=ui.Candidates,commit=commit.TextToOutput??""}));
        }
    }
    static void TimerSpec(string path)
    {
        var regex=(System.Text.RegularExpressions.Regex)typeof(InputMethodEngine).GetField("TimerRegex",BindingFlags.Static|BindingFlags.NonPublic).GetValue(null);
        foreach(var line in File.ReadLines(path)) {
            var code=JsonSerializer.Deserialize<string>(line);
            bool matched=regex.IsMatch(code); int milliseconds=-1;
            // Original scheduling arithmetic, without creating a timer or popup.
            if(matched) {
                string number=code.Substring(2).Replace(",",string.Empty).Replace(" ",string.Empty);
                if(double.TryParse(number,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out double minutes) && minutes>0) {
                    double due=minutes*60d*1000d;
                    if(double.IsNaN(due) || double.IsInfinity(due) || due>int.MaxValue) due=int.MaxValue;
                    milliseconds=(int)due;
                }
            }
            Console.WriteLine(JsonSerializer.Serialize(new {matched,milliseconds}));
        }
    }
    static void Currency(string path)
    {
        var type=typeof(InputMethodEngine);
        var convert=type.GetMethod("ConvertToChineseCurrency",BindingFlags.Static|BindingFlags.NonPublic);
        var prefix=type.GetMethod("IsTimerOrCnum",BindingFlags.Static|BindingFlags.NonPublic);
        foreach(var line in File.ReadLines(path)) {
            var text=JsonSerializer.Deserialize<string>(line);
            Console.WriteLine(JsonSerializer.Serialize(new {text,output=(string)convert.Invoke(null,new object[]{text}),prefix=(bool)prefix.Invoke(null,new object[]{text})}));
        }
    }
    static void Dynamic(CoreRuntimeState state,string path)
    {
        using var engine = new InputMethodEngine(state);
        var convert=typeof(InputMethodEngine).GetMethod("ConvertOutputText",BindingFlags.Instance|BindingFlags.NonPublic);
        object Clock(DateTime t) => new { year=t.Year, month=t.Month, day=t.Day, hour=t.Hour, minute=t.Minute, second=t.Second,
            weekday=(int)t.DayOfWeek, weekdayName=System.Globalization.CultureInfo.CurrentCulture.DateTimeFormat.GetDayName(t.DayOfWeek) };
        foreach(var line in File.ReadLines(path)) {
            var text=JsonSerializer.Deserialize<string>(line);
            var before=Clock(DateTime.Now);
            var output=(string)convert.Invoke(engine,new object[]{text});
            var after=Clock(DateTime.Now);
            Console.WriteLine(JsonSerializer.Serialize(new { text,output,before,after }));
        }
    }
    static void Presentation(string path)
    {
        var formatter = new TigerClaw.Overlay.CandidateTextFormatter();
        foreach (var line in File.ReadLines(path)) {
            var ui = JsonSerializer.Deserialize<TigerClaw.Shared.OverlayUiState>(line);
            Console.WriteLine(JsonSerializer.Serialize(formatter.BuildViewModel(ui, true, true).DisplayText));
        }
    }
    static void Settings(CoreRuntimeState state, string root, string path)
    {
        foreach (string line in File.ReadLines(path))
        {
            File.WriteAllText(Path.Combine(root, "config.txt"), JsonSerializer.Deserialize<string>(line), new UTF8Encoding(true));
            if (!state.ReloadConfig()) throw new InvalidOperationException("Original config reload failed");
            Console.WriteLine(JsonSerializer.Serialize(new {
                defaultChinese=state.GetDefaultChinese(), shiftToggle=state.GetShiftToggleEnabled(),
                ctrlSpaceToggle=state.GetCtrlSpaceToggleEnabled(), englishPunctuation=state.GetUseEnPuncInCn(),
                slashDunhao=state.GetSlashOutputsDunhao(), enterClear=state.GetEnterClear(), tabClear=state.GetTabClear(),
                clearOnNoCode=state.GetClearOnNoCode(), maxCodeAutoCommit=state.GetMaxCodeAutoCommit(),
                reverseLookup=state.GetBackQueryEnabled(), semicolonSecond=state.GetSecondCandidateSemicolon(),
                quoteThird=state.GetThirdCandidateQuote(), showComment=state.GetShowComment(), showSplit=state.GetShowSplit(),
                maxCodeLength=state.GetMaxCodeLength(), pageSize=state.GetPageSize(),
                pageKeys=state.GetPageKeys() switch { "[ ]"=>1, "Shift Tab/Tab"=>2, "PageUp/PageDown"=>3, _=>0 },
                vertical=state.GetVerticalCandidates(), showIndex=state.GetShowCandidateIndex(),
                showCode=state.GetShowInputCodeInCandidateWindow(), hideCandidates=state.GetHideCandidateItems(),
                font=state.GetFontName(), fontSize=state.GetFontSize(), theme=state.GetThemeName()
            }));
        }
    }
    static void Selection(string path)
    {
        const BindingFlags flags = BindingFlags.Static | BindingFlags.NonPublic;
        var type = typeof(InputMethodEngine);
        var defaults = type.GetMethod("BuildDefaultSelectionKeyBindings", flags);
        var parse = type.GetMethod("TryParseCustomSelectionKeyConfigLines", flags);
        var serialize = type.GetMethod("BuildSelectionKeyBindingsText", flags);
        foreach (var line in File.ReadLines(path))
        {
            string text = JsonSerializer.Deserialize<string>(line);
            var lines = new List<string>();
            using (var reader = new StringReader(text)) { string item; while ((item = reader.ReadLine()) != null) lines.Add(item); }
            object baseline = defaults.Invoke(null, null);
            object[] arguments = { lines, baseline, null, null };
            bool valid = (bool)parse.Invoke(null, arguments);
            string canonical = (string)serialize.Invoke(null, new[] { valid ? arguments[2] : baseline });
            Console.WriteLine(JsonSerializer.Serialize(new { valid, canonical }));
        }
    }
    sealed class Change
    {
        public string kind { get; set; }
        public string code { get; set; }
        public string text { get; set; }
        public string[] queries { get; set; }
    }
    static void Changes(CoreRuntimeState state, string path)
    {
        foreach (var line in File.ReadLines(path))
        {
            var op = JsonSerializer.Deserialize<Change>(line);
            bool changed = op.kind switch {
                "add" => state.TryAddCi(op.code, op.text, out _),
                "delete" => state.TryUserDelete(op.code, op.text, out _),
                "top" => state.TryUserTop(op.code, op.text, out _),
                "advance" => state.TryUserAdvance(op.code, op.text, out _),
                _ => throw new ArgumentException("Unknown change kind")
            };
            var main = Field<Dictionary<string, List<string>>>(state, "_lexicon");
            Console.WriteLine(JsonSerializer.Serialize(new { changed,
                quick = (state.IsShortSymbolSemicolonEnabled() ? 1 : 0) | (state.IsShortSymbolSlashEnabled() ? 2 : 0) |
                    (state.IsShortSymbolLBracketEnabled() ? 4 : 0) | (state.IsShortSymbolZEnabled() ? 8 : 0),
                entries = op.queries.Select(key => new { key,
                    flags = (state.IsUniqueTerminalCode(key) ? 1 : 0) | (state.IsNonTerminalCode(key) ? 2 : 0) |
                        (state.IsAutoShortSymbol(key) ? 4 : 0),
                    values = main.TryGetValue(key, out var values) ? values.ToArray() : Array.Empty<string>() }) }));
        }
    }
    static void Export(CoreRuntimeState state, string output)
    {
        var main = Field<Dictionary<string, List<string>>>(state, "_lexicon");
        var prefixes = Field<HashSet<string>>(state, "_nonTerminal");
        var sections = new List<Entry[]>();
        sections.Add(main.Keys.Concat(prefixes).Distinct(StringComparer.Ordinal)
            .OrderBy(x => x, StringComparer.Ordinal)
            .Select(key => new Entry(key,
                (state.IsUniqueTerminalCode(key) ? 1u : 0u) |
                (state.IsNonTerminalCode(key) ? 2u : 0u) |
                (state.IsAutoShortSymbol(key) ? 4u : 0u) |
                (main.ContainsKey(key) ? 8u : 0u),
                main.TryGetValue(key, out var values) ? values.ToArray() : Array.Empty<string>())).ToArray());
        sections.Add(Field<Dictionary<string, List<string>>>(state, "_pinyinLexicon")
            .OrderBy(x => x.Key, StringComparer.Ordinal)
            .Select(x => new Entry(x.Key, 0, x.Value.ToArray())).ToArray());
        foreach (string field in new[] { "_commentMap", "_splitMap", "_fullCodeMap", "_constructCodeMap" })
            sections.Add(Field<Dictionary<string, string>>(state, field)
                .OrderBy(x => x.Key, StringComparer.Ordinal)
                .Select(x => new Entry(x.Key, 0, new[] { x.Value })).ToArray());
        if (main.Count == 0) throw new InvalidDataException("Empty main lexicon; export aborted.");

        string temporary = Path.GetFullPath(output) + "." + Guid.NewGuid().ToString("N") + ".tmp";
        Directory.CreateDirectory(Path.GetDirectoryName(temporary));
        using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None))
        using (var writer = new BinaryWriter(stream, Encoding.Unicode, true))
        {
            writer.Write(Encoding.ASCII.GetBytes("TIGERD02"));
            writer.Write(2u);
            writer.Write((uint)sections.Count);
            writer.Write(0UL); // final file size
            uint quick = (state.IsShortSymbolSemicolonEnabled() ? 1u : 0u) |
                (state.IsShortSymbolSlashEnabled() ? 2u : 0u) |
                (state.IsShortSymbolLBracketEnabled() ? 4u : 0u) |
                (state.IsShortSymbolZEnabled() ? 8u : 0u);
            writer.Write(quick);
            writer.Write(0u);
            long recordsAt = 32 + sections.Count * 16;
            long valuesAt = recordsAt + sections.Sum(x => (long)x.Length * 32);
            long stringsAt = valuesAt + sections.Sum(x => x.Sum(y => (long)y.Values.Length * 16));
            long recordCursor = recordsAt, valueCursor = valuesAt, stringCursor = stringsAt;
            var strings = new Dictionary<string, ulong>(StringComparer.Ordinal);
            ulong Intern(string text)
            {
                if (strings.TryGetValue(text, out ulong found)) return found;
                ulong offset = (ulong)stringCursor;
                long saved = stream.Position;
                stream.Position = stringCursor;
                // Write UTF-16 code units verbatim, including unmatched surrogates.
                foreach (char ch in text) writer.Write((ushort)ch);
                stringCursor = stream.Position;
                stream.Position = saved;
                strings[text] = offset;
                return offset;
            }
            for (int section = 0; section < sections.Count; ++section)
            {
                stream.Position = 32 + section * 16;
                writer.Write((uint)section + 1);
                writer.Write((uint)sections[section].Length);
                writer.Write((ulong)recordCursor);
                foreach (var entry in sections[section])
                {
                    stream.Position = recordCursor;
                    writer.Write(Intern(entry.Key));
                    writer.Write((uint)entry.Key.Length);
                    writer.Write(entry.Flags);
                    writer.Write((ulong)valueCursor);
                    writer.Write((uint)entry.Values.Length);
                    writer.Write(0u);
                    recordCursor += 32;
                    foreach (string value in entry.Values)
                    {
                        stream.Position = valueCursor;
                        writer.Write(Intern(value));
                        writer.Write((uint)value.Length);
                        writer.Write(0u);
                        valueCursor += 16;
                    }
                }
            }
            stream.SetLength(stringCursor);
            stream.Position = 16;
            writer.Write((ulong)stringCursor);
            writer.Flush();
            stream.Flush(true);
        }
        // Each output is a new immutable generation. Never overwrite a mapped file.
        File.Move(temporary, output, false);
        using (var expected = new StreamWriter(output + ".expected.jsonl", false, new UTF8Encoding(false)))
            for (int i = 0; i < sections.Count; ++i)
                foreach (var entry in sections[i])
                    expected.WriteLine(JsonSerializer.Serialize(new { section = i + 1, key = entry.Key, flags = entry.Flags, values = entry.Values }));
        Console.WriteLine(JsonSerializer.Serialize(new { output, bytes = new FileInfo(output).Length,
            counts = sections.Select(x => x.Length), mainCodes = main.Count }));
    }

    sealed class Key
    {
        public int vk { get; set; }
        public int scan { get; set; }
        public string action { get; set; } = "down";
        public bool shift { get; set; }
        public bool ctrl { get; set; }
        public bool alt { get; set; }
        public bool win { get; set; }
        public bool caps { get; set; }
        public bool num { get; set; } = true;
        public int repeat { get; set; } = 1;
        public bool extended { get; set; }
        public bool reset { get; set; }
    }
    static void Trace(CoreRuntimeState state, string path, bool history=false)
    {
        InputMethodEngine engine = new InputMethodEngine(state);
        // Production ProtocolHandler applies the configured language after
        // constructing the core; the engine constructor itself starts Chinese.
        engine.SetChinese(state.GetDefaultChinese(), out _);
        try
        {
            foreach (string line in File.ReadLines(path))
            {
                if (string.IsNullOrWhiteSpace(line)) continue;
                var key = JsonSerializer.Deserialize<Key>(line);
                if (key.reset) { engine.Dispose(); engine = new InputMethodEngine(state); engine.SetChinese(state.GetDefaultChinese(), out _); }
                var result = engine.ProcessKey(key.vk, key.scan, key.action, key.shift, key.ctrl,
                    key.alt, key.win, key.caps, key.num, key.repeat, key.extended);
                engine.PostProcessKey(key.vk, key.action, result, key.shift, key.ctrl, key.alt, key.win, key.caps);
                if(history) Console.WriteLine(JsonSerializer.Serialize(new {history=engine.GetLastCi(20)}));
                else Console.WriteLine(JsonSerializer.Serialize(new { key, result, snapshot = engine.GetDifferentialSnapshot(state.GetPageSize()) }));
            }
        }
        finally { engine.Dispose(); }
    }
}
