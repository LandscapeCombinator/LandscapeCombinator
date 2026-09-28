import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "README.md"
RST_OUT = ROOT / "Documentation" / "source" / "credit.rst"

def convert_md_to_rst(md_text: str) -> str:
    lines = md_text.split("\n")
    rst_lines = []
    in_credit = False
    credit_header_done = False
    header_chars = {"#": "=", "##": "~", "###": "^"}
    
    i = 0
    while i < len(lines):
        line = lines[i]
        
        # Detect credit section start
        if line.strip().startswith("# Credit"):
            in_credit = True
            rst_lines.append("Credit")
            rst_lines.append("=" * len("Credit"))
            rst_lines.append("")
            credit_header_done = True
            i += 1
            continue
        
        # Stop after credit section (next major section)
        if in_credit and line.strip().startswith("# ") and not line.strip().startswith("# Credit"):
            break
        
        if not in_credit:
            i += 1
            continue
        
        # Handle headers
        is_header = False
        for prefix, char in header_chars.items():
            if line.strip().startswith(prefix + " "):
                title = line.strip()[len(prefix):]
                rst_lines.append(title)
                rst_lines.append(char * len(title))
                rst_lines.append("")
                is_header = True
                break
        
        if is_header:
            i += 1
            continue
        
        # Handle links: [text](url) -> `text <url>`_
        line = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'`\1 <\2>`_', line)
        
        # Handle bold: **text** -> **text**
        line = re.sub(r'\*\*([^*]+)\*\*', r'**\1**', line)
        
        # Handle italic: *text* -> *text*
        line = re.sub(r'\*([^*]+)\*', r'*\1*', line)
        
        # Handle unordered lists: * text -> * text
        if line.strip().startswith('* '):
            line = '* ' + line.strip()[2:]
        elif line.strip().startswith('- '):
            line = '* ' + line.strip()[2:]
        
        # Handle empty lines
        if line.strip() == '':
            rst_lines.append('')
        else:
            rst_lines.append(line)
        
        i += 1
    
    return '\n'.join(rst_lines).rstrip() + '\n'


def main():
    if not README.exists():
        print(f"Error: {README} not found")
        return
    
    md_text = README.read_text(encoding="utf-8")
    rst_text = convert_md_to_rst(md_text)
    RST_OUT.write_text(rst_text, encoding="utf-8", newline="\n")
    print(f"Generated {RST_OUT}")


if __name__ == "__main__":
    main()
