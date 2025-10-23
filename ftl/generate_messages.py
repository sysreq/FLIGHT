#!/usr/bin/env python3
"""
Message Code Generator for UART Protocol
Generates C++ message types, views, and builders from YAML schema

Now uses external Jinja2 templates and generates modular message headers.
"""

import yaml
import sys
from pathlib import Path
from jinja2 import Environment, FileSystemLoader
from dataclasses import dataclass
from typing import List

@dataclass
class Field:
    """Represents a message field"""
    type: str
    name: str
    is_string: bool = False
    
    @property
    def cpp_type(self) -> str:
        """Get C++ type for field"""
        if self.is_string:
            return "std::string_view"
        return self.type
    
    @property
    def size_expr(self) -> str:
        """Get size expression for serialization"""
        if self.is_string:
            return f"(1 + {self.name}.size())"  # length byte + data
        
        size_map = {
            'uint8_t': '1', 'int8_t': '1', 'bool': '1',
            'uint16_t': '2', 'int16_t': '2',
            'uint32_t': '4', 'int32_t': '4', 'float': '4',
            'uint64_t': '8', 'int64_t': '8', 'double': '8',
        }
        return size_map.get(self.type, f"sizeof({self.type})")

@dataclass
class Message:
    """Represents a message definition"""
    name: str
    fields: List[Field]
    type_id: int
    
    @property
    def max_size(self) -> str:
        """Calculate maximum message size"""
        fixed_size = 1  # message type byte
        has_variable = False
        
        for field in self.fields:
            if field.is_string:
                has_variable = True
                fixed_size += 1  # just the length byte for sizing
            else:
                size_map = {
                    'uint8_t': 1, 'int8_t': 1, 'bool': 1,
                    'uint16_t': 2, 'int16_t': 2,
                    'uint32_t': 4, 'int32_t': 4, 'float': 4,
                    'uint64_t': 8, 'int64_t': 8, 'double': 8,
                }
                fixed_size += size_map.get(field.type, 4)
        
        if has_variable:
            return f"{fixed_size} + string_data"  # Comment for clarity
        return str(fixed_size)


def parse_yaml(yaml_path: Path) -> List[Message]:
    """Parse YAML file and create Message objects"""
    with open(yaml_path, 'r') as f:
        data = yaml.safe_load(f)
    
    messages = []
    for idx, msg_def in enumerate(data['messages']):
        fields = []
        for field_def in msg_def['fields']:
            is_string = field_def['type'] == 'string'
            fields.append(Field(
                type=field_def['type'],
                name=field_def['name'],
                is_string=is_string
            ))
        
        messages.append(Message(
            name=msg_def['name'],
            fields=fields,
            type_id=idx
        ))
    
    return messages


def setup_jinja_env(template_dir: Path) -> Environment:
    """Setup Jinja2 environment with template directory"""
    if not template_dir.exists():
        raise FileNotFoundError(f"Template directory not found: {template_dir}")
    
    env = Environment(
        loader=FileSystemLoader(template_dir),
        trim_blocks=True,
        lstrip_blocks=True
    )
    return env


def generate_files(messages: List[Message], output_dir: Path, template_dir: Path):
    """Generate all C++ files from templates"""
    
    # Setup Jinja2
    env = setup_jinja_env(template_dir)
    use_expected = True  # Use C++23 std::expected
    
    # Create output directories
    output_dir.mkdir(parents=True, exist_ok=True)
    messages_dir = output_dir / "messages"
    messages_dir.mkdir(exist_ok=True)
    
    # Shared template context
    context = {
        'messages': messages,
        'use_expected': use_expected
    }
    
    print(f"Generating files in {output_dir}...")
    
    # 1. Generate messages_detail.h
    print("  - messages_detail.h")
    template = env.get_template('messages_detail.h.j2')
    content = template.render(context)
    (output_dir / 'messages_detail.h').write_text(content)
    
    # 2. Generate individual message headers
    message_template = env.get_template('message.h.j2')
    for msg in messages:
        print(f"  - messages/{msg.name}.h")
        msg_context = {**context, 'msg': msg}
        content = message_template.render(msg_context)
        (messages_dir / f'{msg.name}.h').write_text(content)
    
    # 3. Generate main messages.h
    print("  - messages.h")
    template = env.get_template('messages.h.j2')
    content = template.render(context)
    (output_dir / 'messages.h').write_text(content)
    
    # 4. Generate messages.cpp
    print("  - messages.cpp")
    template = env.get_template('messages.cpp.j2')
    content = template.render(context)
    (output_dir / 'messages.cpp').write_text(content)
    
    print(f"\nGenerated {len(messages) + 3} files successfully!")


def main():
    if len(sys.argv) < 2:
        print("Usage: generate_messages.py <messages.yaml> [output_dir] [template_dir]")
        print("\nGenerates modular C++ message code from YAML definitions.")
        print("  messages.yaml  - Input YAML file with message definitions")
        print("  output_dir     - Output directory (default: current directory)")
        print("  template_dir   - Template directory (default: ./templates)")
        sys.exit(1)
    
    yaml_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path.cwd()
    template_dir = Path(sys.argv[3]) if len(sys.argv) > 3 else Path('templates')
    
    if not yaml_path.exists():
        print(f"Error: {yaml_path} not found")
        sys.exit(1)
    
    print(f"Parsing {yaml_path}...")
    messages = parse_yaml(yaml_path)
    print(f"Found {len(messages)} message types")
    
    # Generate all files
    generate_files(messages, output_dir, template_dir)
    
    print("\nGeneration complete!")
    print("\nOutput structure:")
    print("  messages_detail.h         - Common types and helpers")
    print("  messages.h                - Main header (includes all messages)")
    print("  messages.cpp              - Implementation")
    print("  messages/")
    for msg in messages:
        print(f"    {msg.name}.h          - {msg.name} message type")
    print("\nNext steps:")
    print("1. Add messages.cpp to your build system")
    print("2. Include messages.h in your application code")
    print("3. Ensure g_message_pool is defined in your uart_api.cpp")


if __name__ == '__main__':
    main()
