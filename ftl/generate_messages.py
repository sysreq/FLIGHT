#!/usr/bin/env python3
"""
Message Code Generator for UART Protocol
Generates C++ message types, views, and builders from YAML schema

Now supports:
- Primitive types (uint8_t, float, etc.)
- String types (length-prefixed)
- Array types (uint32_t[5], float[10], etc.)
"""

import yaml
import sys
import re
from pathlib import Path
from jinja2 import Environment, FileSystemLoader
from dataclasses import dataclass
from typing import List, Optional

@dataclass
class Field:
    """Represents a message field"""
    type: str
    name: str
    offset: int = 0
    is_string: bool = False
    is_array: bool = False
    array_size: int = 0
    element_type: str = ""

    @classmethod
    def from_yaml(cls, field_def: dict) -> 'Field':
        """Parse field from YAML definition"""
        type_str = field_def['type']
        name = field_def['name']
        
        # Check for string type
        if type_str == 'string':
            return cls(
                type=type_str,
                name=name,
                is_string=True
            )
        
        # Check for array type: "uint32_t[5]" or "float[10]"
        array_match = re.match(r'(\w+)\[(\d+)\]', type_str)
        if array_match:
            element_type = array_match.group(1)
            array_size = int(array_match.group(2))
            return cls(
                type=type_str,
                name=name,
                is_array=True,
                array_size=array_size,
                element_type=element_type
            )
        
        # Regular primitive type
        return cls(
            type=type_str,
            name=name
        )

    @property
    def cpp_type(self) -> str:
        """Get C++ type for field"""
        if self.is_string:
            return "std::string_view"
        elif self.is_array:
            return f"std::span<const {self.element_type}>"
        return self.type

    @property
    def builder_param_type(self) -> str:
        """Get C++ type for builder parameter"""
        if self.is_string:
            return "std::string_view"
        elif self.is_array:
            return f"std::span<const {self.element_type}, {self.array_size}>"
        return self.type

    @property
    def static_size(self) -> int:
        """Get static size of the field in bytes"""
        if self.is_string:
            return 1  # Length byte
        
        size_map = {
            'uint8_t': 1, 'int8_t': 1, 'bool': 1,
            'uint16_t': 2, 'int16_t': 2,
            'uint32_t': 4, 'int32_t': 4, 'float': 4,
            'uint64_t': 8, 'int64_t': 8, 'double': 8,
        }

        if self.is_array:
            element_size = size_map.get(self.element_type, 0)
            if element_size == 0:
                raise ValueError(f"Unknown element type: {self.element_type}")
            return element_size * self.array_size
        else:
            size = size_map.get(self.type, 0)
            if size == 0:
                raise ValueError(f"Unknown type: {self.type}")
            return size

@dataclass
class Message:
    """Represents a message definition"""
    name: str
    fields: List[Field]
    type_id: int
    has_dynamic_fields: bool = False

    @property
    def static_size(self) -> int:
        """Calculate total size of all fixed-size fields"""
        size = 1  # message type byte
        for field in self.fields:
            if not field.is_string:
                size += field.static_size
        return size


def parse_yaml(yaml_path: Path) -> List[Message]:
    """Parse YAML file and create Message objects"""
    with open(yaml_path, 'r') as f:
        data = yaml.safe_load(f)
    
    messages = []
    for idx, msg_def in enumerate(data['messages']):
        fields = [Field.from_yaml(field_def) for field_def in msg_def['fields']]
        
        # Pre-calculate offsets
        current_offset = 1  # Start after message type byte
        has_dynamic = False
        for field in fields:
            if has_dynamic:
                field.offset = -1 # Dynamic offset
            else:
                field.offset = current_offset

            if field.is_string:
                has_dynamic = True
                # For dynamic fields, subsequent field offsets are also dynamic.
                # The static offset calculation only considers the length byte of the string.
                current_offset += 1
            else:
                current_offset += field.static_size

        messages.append(Message(
            name=msg_def['name'],
            fields=fields,
            type_id=idx,
            has_dynamic_fields=has_dynamic
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
    
    # 1. Generate serialization.h
    print("  - serialization.h")
    template = env.get_template('serialization.h.j2')
    content = template.render(context)
    (output_dir / 'serialization.h').write_text(content)

    # 2. Generate messages_detail.h
    print("  - messages_detail.h")
    template = env.get_template('messages_detail.h.j2')
    content = template.render(context)
    (output_dir / 'messages_detail.h').write_text(content)
    
    # 3. Generate individual message headers
    message_template = env.get_template('message.h.j2')
    for msg in messages:
        print(f"  - messages/{msg.name}.h")
        msg_context = {**context, 'msg': msg}
        content = message_template.render(msg_context)
        (messages_dir / f'{msg.name}.h').write_text(content)
    
    # 4. Generate main messages.h
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
        print("\nSupported field types:")
        print("  - Primitives: uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t")
        print("                uint64_t, int64_t, float, double, bool")
        print("  - Strings:    string (length-prefixed)")
        print("  - Arrays:     <type>[<size>]  (e.g., float[5], uint32_t[10])")
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
    print("3. Ensure g_message_pool is defined in your ftl_api.cpp")


if __name__ == '__main__':
    main()
