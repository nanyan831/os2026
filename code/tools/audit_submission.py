#!/usr/bin/env python3
"""Audit source/test preservation, report links, PNGs, and a source-only ZIP."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def test_text(path, section):
    text = path.read_text(encoding='utf-8')
    if section == 'best-fit-checks':
        return text[text.index('// Framework tests:'):]
    if section == 'first-fit-checks':
        begin = re.search(r'static void\s+basic_check\(void\)', text).start()
        end = text.index('const struct pmm_manager default_pmm_manager')
        return text[begin:end]
    return text


def png_info(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    width, height = struct.unpack('>II', data[16:24])
    offset = 8
    while offset < len(data):
        size = struct.unpack('>I', data[offset:offset + 4])[0]
        block = data[offset + 4:offset + 8 + size]
        checksum = struct.unpack('>I', data[offset + 8 + size:offset + 12 + size])[0]
        assert zlib.crc32(block) & 0xffffffff == checksum, f'PNG CRC failed: {path}'
        offset += size + 12
    assert offset == len(data) and width >= 1000 and height >= 400
    return {'path': str(path.relative_to(ROOT)), 'width': width, 'height': height, 'sha256': sha(data)}


def included_files():
    excluded = {'.git', 'bin', 'obj', '__pycache__'}
    result = []
    for path in ROOT.rglob('*'):
        relative = path.relative_to(ROOT)
        if any(part in excluded for part in relative.parts) or not path.is_file():
            continue
        if path.suffix in {'.pyc', '.swp', '.swo'}:
            continue
        assert path.suffix.lower() not in {'.exe', '.dll', '.xz'}
        result.append(path)
    return sorted(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--reference', type=Path, default=ROOT.parent / 'lab2/lab2_new')
    args = parser.parse_args()
    fixture_file = ROOT / 'code/tools/course_fixture_hashes.json'
    fixtures = json.loads(fixture_file.read_text())
    fixture_results = []
    for item in fixtures:
        text = test_text(ROOT / 'code' / item['path'], item['section'])
        actual = sha(text.encode('utf-8'))
        assert actual == item['sha256'], f'Course fixed test modified: {item}'
        fixture_results.append({'path': item['path'], 'section': item['section'], 'sha256': actual, 'unchanged': True})
    assert '#include <test_lab2.h>' not in (ROOT / 'code/kern/mm/pmm.c').read_text()
    assert 'kern/test' not in (ROOT / 'code/Makefile').read_text()
    for path in (ROOT / 'code/kern').rglob('*'):
        if path.suffix in {'.c', '.h', '.S'}:
            assert 'YOUR CODE' not in path.read_text(encoding='utf-8'), path
    prompts = sorted((ROOT / 'report/prompts').glob('*.md'))
    assert len(prompts) == 10
    for path in prompts:
        text = path.read_text(encoding='utf-8')
        for tag in ['[PROMPT]', '[RELY]', '[GUARANTEE]', '[SPECIFICATION]']:
            assert tag in text, f'Missing {tag}: {path}'
    report = (ROOT / 'report/report.md').read_text(encoding='utf-8')
    for section in ['## 一、实验目的', '## 二、实验环境', '## 三、实验整体逻辑分析',
                    '## 四、实验内容与实现', '## 五、测试与验证', '## 六、实验总结与收获']:
        assert section in report
    assert report.count('[GUARANTEE]') >= 10
    for path in ROOT.rglob('*.md'):
        if '.git' in path.parts:
            continue
        for target in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', path.read_text(encoding='utf-8')):
            if target.startswith(('http:', 'https:', '#', 'mailto:')):
                continue
            local = target.split('#', 1)[0]
            assert (path.parent / local).exists(), f'Broken local link: {path}: {local}'
    images = [png_info(path) for path in sorted((ROOT / 'report/images').glob('*.png'))]
    assert len(images) == 7
    matrices = []
    for relative in ['verification-all.json', 'qemu7/verification-all.json', 'verification-qemu.json']:
        data = json.loads((ROOT / 'report/logs' / relative).read_text())
        assert all(item['passed'] for item in data['results'])
        matrices.append({'path': relative, 'passed': len(data['results'])})
    assert [item['passed'] for item in matrices] == [11, 11, 3]
    assert 'Total Score: 20/20' in (ROOT / 'report/logs/official-grade.log').read_text()
    assert 'Total Score: 20/20' in (ROOT / 'report/logs/qemu7/official-grade.log').read_text()
    assert 'HIGH KERNEL ENTRY' in (ROOT / 'report/logs/paging-trace.log').read_text()
    header = subprocess.check_output(['riscv64-unknown-elf-readelf', '-h', 'bin/kernel'], cwd=ROOT / 'code', text=True)
    assert 'ELF64' in header and 'RISC-V' in header
    (ROOT / 'report/logs/kernel-elf-header.log').write_text(header)
    reference_count = None
    original_zip = ROOT.parent / 'lab2.zip'
    if args.reference.is_dir() and original_zip.is_file():
        reference_count = 0
        with zipfile.ZipFile(original_zip) as supplied:
            for path in args.reference.rglob('*'):
                if not path.is_file():
                    continue
                relative = path.relative_to(args.reference).as_posix()
                names = [name for name in supplied.namelist() if name == relative or name.endswith('/' + relative)]
                assert len(names) == 1, relative
                assert supplied.read(names[0]) == path.read_bytes(), f'Original template changed: {relative}'
                reference_count += 1
    secret = re.compile(rb'(?:sk-[A-Za-z0-9]{24,}|ghp_[A-Za-z0-9]{24,}|github_pat_[A-Za-z0-9_]{30,})')
    whitespace = json.loads((ROOT / 'code/tools/template_whitespace.json').read_text())
    inherited_whitespace = 0
    for path in included_files():
        if path.suffix != '.png':
            assert secret.search(path.read_bytes()) is None, f'Possible credential: {path}'
            relative = path.relative_to(ROOT).as_posix()
            if relative.startswith('report/logs/'):
                continue  # Preserve raw tool output, including its whitespace.
            text = path.read_text(encoding='utf-8')
            baseline = whitespace.get(relative.removeprefix('code/'), {})
            for number, line in enumerate(text.splitlines(), 1):
                if line.rstrip(' \t') != line:
                    assert line in baseline.get('trailing_lines', []), f'New trailing whitespace: {relative}:{number}'
                    inherited_whitespace += 1
            if text.endswith('\n\n'):
                assert baseline.get('blank_at_eof', False), f'New blank EOF: {relative}'
                inherited_whitespace += 1
    summary = {'date': '2026-10-18', 'fixed_tests': fixture_results, 'prompts': len(prompts),
               'template_sections': 6, 'images': images, 'matrices': matrices,
               'kernel': 'ELF64 RISC-V', 'original_template_files_unchanged': reference_count,
               'grading_injection_clean': True, 'report_links_valid': True,
               'source_whitespace': {'new_issues': 0, 'inherited_template_findings': inherited_whitespace,
                                     'raw_tool_logs_preserved': True},
               'remaining_metadata': []}
    audit_path = ROOT / 'report/audit.json'
    audit_path.write_text(json.dumps(summary, indent=2))
    git = ['git']
    git_link = ROOT / '.git'
    if git_link.is_file() and re.match(r'gitdir: [A-Za-z]:[/\\]', git_link.read_text()):
        # A Windows-created worktree contains a Windows gitdir; keep it valid
        # for Windows Git instead of rewriting shared repository metadata.
        windows_git = shutil.which('git.exe')
        assert windows_git, 'Use Windows Git for this Windows-created worktree'
        windows_root = subprocess.check_output(['wslpath', '-w', str(ROOT)], text=True).strip()
        git = [windows_git, '-C', windows_root]
    # Original fixed fixtures contain trailing spaces/blank EOFs; the source
    # comparison above checks all new lines without rewriting those fixtures.
    subprocess.run(git + ['status', '--porcelain'], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    if args.archive:
        files = included_files()
        hashes = {path.relative_to(ROOT).as_posix(): sha(path.read_bytes()) for path in files}
        manifest = ''.join(value + '  ' + name + '\n' for name, value in hashes.items())
        with zipfile.ZipFile(args.archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in files:
                archive.write(path, path.relative_to(ROOT).as_posix())
            archive.writestr('SHA256SUMS.txt', manifest)
        with zipfile.ZipFile(args.archive) as archive:
            assert archive.testzip() is None
            for name, digest in hashes.items():
                assert sha(archive.read(name)) == digest, name
        checksum = sha(args.archive.read_bytes())
        Path(str(args.archive) + '.sha256').write_text(checksum + '  ' + args.archive.name + '\n')
        print(f'ARCHIVE VERIFIED: {len(files) + 1} entries; CRC and every SHA-256 match; sha256={checksum}')
    print(f'AUDIT PASS: {len(fixture_results)} fixed test fixtures, 10 prompts, 7 PNGs, 22 runtime scenarios + 3 host-debug scenarios.')


if __name__ == '__main__':
    main()
