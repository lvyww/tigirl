"""Folder selection through the actual hidden manager, importer and selector."""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
from windows_process import run_windows, PS
ROOT=Path(__file__).resolve().parents[1]; BUILD=ROOT/'build'; TOOLS=BUILD/'tests/ARM64'
def win(p): return subprocess.check_output(['wslpath','-w',str(p)],text=True).strip()
def run(tool,*args,success=True):
    r=run_windows([TOOLS/(tool+'.exe'),*args],capture_output=True,text=True,timeout=60)
    assert (r.returncode==0)==success,(tool,r.stdout,r.stderr)
    return r
with tempfile.TemporaryDirectory(prefix='folder-schema-',dir=BUILD) as temporary:
    root=Path(temporary); user=root/'用户';user.mkdir();(user/'.schema-manager-test').touch()
    sources=root/'码表';sources.mkdir();source=sources/'虎码字词';source.mkdir()
    table=source/'词条.txt';table.write_text('ab 初次内容\n',encoding='utf-8')
    pinyin=root/'拼音反查码表';pinyin.mkdir();(pinyin/'拼音.txt').write_text('ce 测\n',encoding='utf-8')
    config=user/'config.txt';config.write_text('码表存储位置\t'+win(sources)+'\n',encoding='utf-8')
    def select(name): return run('schema_manager',win(user),win(ROOT/'data/tiger-v2.tcd'),'--test-folder',name)
    def ensure(success=True):return run('lexicon_import','--ensure',win(source),win(pinyin),win(user),'虎码字词','zh-CN',success=success)
    def selected():
        gen=(user/'schemas/虎码字词/current.txt').read_text(encoding='utf-8-sig').strip().split('\t')[1]
        return user/'schemas/虎码字词/generations'/gen/'tiger-v2.tcd'
    select('虎码字词');first=selected();first_bytes=first.read_bytes()
    assert '当前码表\t虎码字词' in config.read_text(encoding='utf-8-sig')
    assert (user/'user/tiger-words.tcu').exists() and not (user/'schemas/虎码字词/user.tcu').exists()
    run('user_store_probe',win(first),win(user/'user/tiger-words.tcu'),'write','ab','kept-word-',2)
    journal=(user/'user/tiger-words.tcu').read_bytes()
    assert json.loads(ensure().stdout)['reused'] and selected()==first
    stamp=table.stat();table.write_text('ab 更新内容\n',encoding='utf-8');os.utime(table,ns=(stamp.st_atime_ns,stamp.st_mtime_ns))
    select('虎码字词');second=selected();assert second!=first and second.read_bytes()!=first_bytes and first.read_bytes()==first_bytes
    assert (user/'user/tiger-words.tcu').read_bytes()==journal
    words=run('user_store_probe',win(second),win(user/'user/tiger-words.tcu'),'dump','ab').stdout
    assert ''.join(f'{ord(c):04x}' for c in 'kept-word-0') in words
    (pinyin/'拼音.txt').write_text('ce 策\n',encoding='utf-8');ensure();third=selected();assert third!=second
    before=(user/'schemas/虎码字词/current.txt').read_bytes();saved=config.read_bytes()
    table.rename(source/'temporary.bak');source.rename(sources/'temporarily-removed')
    ensure(success=False);assert (user/'schemas/虎码字词/current.txt').read_bytes()==before and config.read_bytes()==saved
    (sources/'temporarily-removed').rename(source);(source/'temporary.bak').rename(table)
    # A failed cache-metadata replacement must not publish the new generation.
    metadata=user/'schemas/虎码字词/source-cache.txt'
    def readonly(enabled):
        quoted="'"+win(metadata).replace("'","''")+"'"
        code='[IO.File]::SetAttributes('+quoted+',[IO.FileAttributes]::'+('ReadOnly' if enabled else 'Normal')+')'
        subprocess.run(['/init',PS,'-NoProfile','-Command',code],check=True,capture_output=True)
    original_table=table.read_bytes();table.write_text('ab 不应发布\n',encoding='utf-8')
    readonly(True)
    try:
        ensure(success=False)
        assert (user/'schemas/虎码字词/current.txt').read_bytes()==before and config.read_bytes()==saved
    finally:
        readonly(False);table.write_bytes(original_table)
    # New folder appears directly without manual import/name entry.
    other=sources/'普通方案';other.mkdir();(other/'词条.txt').write_text('ab 另一方案\n',encoding='utf-8')
    select('普通方案');assert '当前码表\t普通方案' in config.read_text(encoding='utf-8-sig')
    assert (user/'schemas/普通方案/user.tcu').exists()
    select('虎码字词');assert selected()==third
    # Invalid live cache must be rejected by selection, not silently use bundled data.
    third.write_bytes(b'broken');saved=config.read_bytes()
    run('schema_select',win(user),win(ROOT/'data/tiger-v2.tcd'),'虎码字词',success=False)
    assert config.read_bytes()==saved
    ensure();assert selected()!=third
    report=dict(status='passed',folder_ui=True,builtin_name_override=True,existing_journal_preserved=True,
        new_folder_discovery=True,content_change_with_same_timestamp=True,pinyin_change=True,
        unchanged_cache_reused=True,failed_load_preserved=True,metadata_failure_preserved=True,invalid_cache_rebuilt=True,
        hidden_layout_dpis=[96,144,192],physical_input_tested=False,
        hashes={n:hashlib.sha256((TOOLS/(n+'.exe')).read_bytes()).hexdigest() for n in ['schema_manager','schema_select','lexicon_import']})
(BUILD/'schema-folder-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
