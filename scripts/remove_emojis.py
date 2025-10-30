#!/usr/bin/env python3
"""
Remove emojis and special symbols from markdown files
Replace with readable ASCII text
"""

import re
import os
import sys

# Emoji to text mappings
EMOJI_MAP = {
    '✅': '[OK]',
    '❌': '[X]',
    '⚠️': '[WARNING]',
    '🎯': '[TARGET]',
    '📊': '[TABLE]',
    '🔧': '[CONFIG]',
    '💻': '[CODE]',
    '📝': '[NOTE]',
    '🚀': '[START]',
    '🔄': '[UPDATE]',
    '📋': '[LIST]',
    '🔍': '[SEARCH]',
    '💡': '[INFO]',
    '🎓': '[LEARN]',
    '🏗️': '[BUILD]',
    '📚': '[DOCS]',
    '🔐': '[SECURITY]',
    '📦': '[PACKAGE]',
    '🌐': '[NETWORK]',
    '🔌': '[CONNECT]',
    '📡': '[SIGNAL]',
    '⏳': '[PENDING]',
    '🚨': '[ALERT]',
    '🎉': '[SUCCESS]',
    '🤔': '[QUESTION]',
    '💾': '[STORAGE]',
    '🗂️': '[FILE]',
    '📐': '[DESIGN]',
    '🔑': '[KEY]',
    '🛠️': '[TOOL]',
    '📖': '[REFERENCE]',
    '🏷️': '[TAG]',
    '🔒': '[SECURE]',
    '🐳': '[DOCKER]',
    '⏰': '[TIME]',
    '🎬': '[ACTION]',
    '👏': '',
    '👍': '',
    '🙏': '',
    '😄': '',
    '😊': '',
    '🤯': '',
    '🎭': '[EXAMPLE]',
    '🔥': '[HOT]',
    '✨': '',
    '🧮': '[CALC]',
    '🧪': '[TEST]',
    '🚦': '[CONTROL]',
    '📅': '[DATE]',
    '🏁': '[FINISH]',
    '💥': '[ERROR]',
    '🔵': '[SERVER]',
    '🔴': '[CLIENT]',
    '━': '-',
    '┌': '+',
    '┐': '+',
    '└': '+',
    '┘': '+',
    '├': '+',
    '┤': '+',
    '┬': '+',
    '┴': '+',
    '┼': '+',
    '│': '|',
    '─': '-',
    '▼': 'v',
    '►': '>',
    '◄': '<',
    '▲': '^',
    '⇄': '<->',
    '→': '->',
    '←': '<-',
    '↓': '|',
    '↑': '|',
    '⇒': '=>',
}

def remove_emojis_from_text(text):
    """Remove emojis and replace with text"""
    for emoji, replacement in EMOJI_MAP.items():
        text = text.replace(emoji, replacement)
    
    # Remove any remaining emojis (unicode emoji range)
    emoji_pattern = re.compile(
        "["
        "\U0001F600-\U0001F64F"  # emoticons
        "\U0001F300-\U0001F5FF"  # symbols & pictographs
        "\U0001F680-\U0001F6FF"  # transport & map symbols
        "\U0001F1E0-\U0001F1FF"  # flags
        "\U00002702-\U000027B0"
        "\U000024C2-\U0001F251"
        "]+",
        flags=re.UNICODE
    )
    text = emoji_pattern.sub('', text)
    
    return text

def clean_markdown_file(filepath):
    """Clean a single markdown file"""
    print(f"Processing: {filepath}")
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    content = remove_emojis_from_text(content)
    
    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  -> Cleaned")
    else:
        print(f"  -> No changes needed")

def main():
    if len(sys.argv) > 1:
        # Process specific file
        clean_markdown_file(sys.argv[1])
    else:
        # Process all .md files in docs directory
        docs_dir = os.path.join(os.path.dirname(__file__), '..', 'docs')
        for filename in os.listdir(docs_dir):
            if filename.endswith('.md'):
                filepath = os.path.join(docs_dir, filename)
                clean_markdown_file(filepath)

if __name__ == '__main__':
    main()

