#!/usr/bin/env python3
"""
BLE Protocol Documentation Generator

This script parses the BLE service source files and generates comprehensive
protocol documentation based on the typed packet structures and service definitions.
"""

import os
import re
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass

@dataclass
class PacketField:
    name: str
    type: str
    size: int
    offset: int
    description: str = ""

@dataclass
class PacketStruct:
    name: str
    total_size: int
    fields: List[PacketField]
    description: str = ""

@dataclass
class Characteristic:
    name: str
    uuid: str
    properties: List[str]
    permissions: List[str]
    read_packet: Optional[str] = None
    write_packet: Optional[str] = None
    description: str = ""

@dataclass
class Service:
    name: str
    uuid: str
    characteristics: List[Characteristic]
    description: str = ""

class BLEProtocolParser:
    def __init__(self, services_dir: str):
        self.services_dir = Path(services_dir)
        self.services: List[Service] = []
        self.packet_structs: Dict[str, PacketStruct] = {}
        self.uuid_definitions: Dict[str, str] = {}
        
        # Type size mapping
        self.type_sizes = {
            'uint8_t': 1,
            'uint16_t': 2,  
            'uint32_t': 4,
            'int8_t': 1,
            'int16_t': 2,
            'int32_t': 4,
            'char': 1,
            'bool': 1
        }
        
        # Standard Bluetooth SIG UUIDs
        self.standard_uuids = {
            'BT_UUID_DIS': '0x180A',
            'BT_UUID_DIS_MANUFACTURER_NAME': '0x2A29',
            'BT_UUID_DIS_MODEL_NUMBER': '0x2A24',
            'BT_UUID_DIS_FIRMWARE_REVISION': '0x2A26',
            'BT_UUID_DIS_HARDWARE_REVISION': '0x2A27', 
            'BT_UUID_DIS_SOFTWARE_REVISION': '0x2A28'
        }

    def parse_all_services(self):
        """Parse all BLE service files in the services directory."""
        # First, parse all header files for UUID definitions and packet structures
        self.parse_uuid_definitions()
        self.parse_packet_structures_from_headers()
        
        service_files = list(self.services_dir.glob("*_service.c"))
        
        for file_path in service_files:
            print(f"Parsing {file_path.name}...")
            self.parse_service_file(file_path)
    
    def parse_packet_structures_from_headers(self):
        """Parse packet structures from header files."""
        header_files = list(self.services_dir.glob("*_service.h"))
        
        for file_path in header_files:
            print(f"Parsing packet structures from {file_path.name}...")
            with open(file_path, 'r') as f:
                content = f.read()
            self.extract_packet_structs(content)
    
    def parse_uuid_definitions(self):
        """Parse UUID definitions from header files."""
        header_files = list(self.services_dir.glob("*_service.h"))
        
        for file_path in header_files:
            with open(file_path, 'r') as f:
                content = f.read()
            self.extract_uuid_definitions(content)
    
    def extract_uuid_definitions(self, content: str):
        """Extract UUID definitions from header file content."""
        # Pattern for #define UUID_NAME BT_UUID_128(...)
        uuid_128_pattern = r'#define\s+([A-Z_]+UUID[A-Z_]*)\s+BT_UUID_128\(BT_UUID_128_ENCODE\(([^)]+)\)\)'
        
        for match in re.finditer(uuid_128_pattern, content):
            uuid_name = match.group(1)
            uuid_params = match.group(2)
            
            # Parse the UUID parameters (0x12345678, 0x1234, 0x5678, 0x1234, 0x123456789ABC)
            params = [p.strip() for p in uuid_params.split(',')]
            if len(params) >= 5:
                # Convert to standard UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
                try:
                    uuid_str = self.format_128_bit_uuid(params)
                    self.uuid_definitions[uuid_name] = uuid_str
                except:
                    # If parsing fails, keep the raw definition
                    self.uuid_definitions[uuid_name] = f"BT_UUID_128({uuid_params})"
        
        # Pattern for #define UUID_NAME BT_UUID_16(0xXXXX)
        uuid_16_pattern = r'#define\s+([A-Z_]+UUID[A-Z_]*)\s+BT_UUID_16\(([^)]+)\)'
        
        for match in re.finditer(uuid_16_pattern, content):
            uuid_name = match.group(1)
            uuid_value = match.group(2).strip()
            self.uuid_definitions[uuid_name] = uuid_value
    
    def format_128_bit_uuid(self, params: List[str]) -> str:
        """Format 128-bit UUID parameters into standard UUID string."""
        # Remove 0x prefix and convert to proper format
        cleaned_params = []
        for param in params:
            param = param.strip().replace('0x', '').replace('0X', '')
            cleaned_params.append(param)
        
        # Standard UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        # params are: [32-bit, 16-bit, 16-bit, 16-bit, 48-bit]
        part1 = cleaned_params[0].zfill(8)  # 32-bit
        part2 = cleaned_params[1].zfill(4)  # 16-bit  
        part3 = cleaned_params[2].zfill(4)  # 16-bit
        part4 = cleaned_params[3].zfill(4)  # 16-bit
        part5 = cleaned_params[4].zfill(12) # 48-bit
        
        return f"{part1}-{part2}-{part3}-{part4}-{part5}".upper()
    
    def resolve_uuid(self, uuid_ref: str) -> str:
        """Resolve a UUID reference to its actual value."""
        # Clean up the reference
        uuid_ref = uuid_ref.strip()
        
        # Check if it's already a resolved UUID (contains hyphens or 0x)
        if '-' in uuid_ref or uuid_ref.startswith('0x'):
            return uuid_ref
        
        # Check standard UUIDs first
        if uuid_ref in self.standard_uuids:
            return self.standard_uuids[uuid_ref]
        
        # Check parsed definitions
        if uuid_ref in self.uuid_definitions:
            return self.uuid_definitions[uuid_ref]
        
        # If not found, return as-is with a note
        return f"{uuid_ref} (undefined)"
    
    def parse_service_file(self, file_path: Path):
        """Parse a single BLE service file."""
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Extract packet structures
        self.extract_packet_structs(content)
        
        # Extract service definition
        service = self.extract_service_definition(content, file_path.stem)
        if service:
            self.services.append(service)
    
    def extract_packet_structs(self, content: str):
        """Extract typed packet structures from the file content."""
        # Pattern to match typedef struct definitions
        struct_pattern = r'typedef\s+struct\s*\{([^}]+)\}\s*__attribute__\(\(packed\)\)\s*([a-zA-Z_][a-zA-Z0-9_]*);'
        
        for match in re.finditer(struct_pattern, content, re.MULTILINE | re.DOTALL):
            struct_body = match.group(1).strip()
            struct_name = match.group(2).strip()
            
            fields = self.parse_struct_fields(struct_body)
            total_size = sum(field.size for field in fields)
            
            # Extract description from comment above struct
            description = self.extract_comment_above(content, match.start())
            
            packet_struct = PacketStruct(
                name=struct_name,
                total_size=total_size,
                fields=fields,
                description=description
            )
            
            self.packet_structs[struct_name] = packet_struct
    
    def parse_struct_fields(self, struct_body: str) -> List[PacketField]:
        """Parse fields within a struct definition."""
        fields = []
        current_offset = 0
        
        # Split by lines and process each field
        lines = [line.strip() for line in struct_body.split('\n') if line.strip()]
        
        for line in lines:
            # Skip comments
            if line.startswith('//') or line.startswith('/*'):
                continue
            
            # Remove trailing comment and semicolon
            line = re.sub(r'//.*$', '', line).strip()
            line = line.rstrip(';')
            
            if not line:
                continue
            
            # Parse field: type name[size]; or type name;
            field_match = re.match(r'([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:\[([0-9]+)\])?', line)
            if field_match:
                field_type = field_match.group(1)
                field_name = field_match.group(2)
                array_size = field_match.group(3)
                
                # Calculate field size
                base_size = self.type_sizes.get(field_type, 1)
                if array_size:
                    field_size = base_size * int(array_size)
                    field_type = f"{field_type}[{array_size}]"
                else:
                    field_size = base_size
                
                # Extract description from inline comment
                original_line = line
                description = ""
                comment_match = re.search(r'//\s*(.+)$', original_line)
                if comment_match:
                    description = comment_match.group(1).strip()
                
                field = PacketField(
                    name=field_name,
                    type=field_type,
                    size=field_size,
                    offset=current_offset,
                    description=description
                )
                
                fields.append(field)
                current_offset += field_size
        
        return fields
    
    def extract_service_definition(self, content: str, file_prefix: str) -> Optional[Service]:
        """Extract BLE service definition from the file content."""
        # Find BT_GATT_SERVICE_DEFINE
        service_pattern = r'BT_GATT_SERVICE_DEFINE\s*\(\s*([^,]+),([^;]+)\);'
        service_match = re.search(service_pattern, content, re.MULTILINE | re.DOTALL)
        
        if not service_match:
            return None
        
        service_name = service_match.group(1).strip()
        service_body = service_match.group(2).strip()
        
        # Extract service UUID from BT_GATT_PRIMARY_SERVICE
        uuid_pattern = r'BT_GATT_PRIMARY_SERVICE\s*\(\s*([^)]+)\s*\)'
        uuid_match = re.search(uuid_pattern, service_body)
        service_uuid_ref = uuid_match.group(1).strip() if uuid_match else "Unknown"
        service_uuid = self.resolve_uuid(service_uuid_ref)
        
        # Extract characteristics
        characteristics = self.extract_characteristics(service_body, content)
        
        # Generate description based on file name
        service_descriptions = {
            'device_info_service': 'Standard Device Information Service providing device identification',
            'dfu_service': 'Device Firmware Update Service for over-the-air firmware updates',
            'control_service': 'Custom Control Service for device command and control operations',
            'data_service': 'Custom Data Service for bidirectional data transfer operations'
        }
        
        description = service_descriptions.get(file_prefix, f"BLE service: {service_name}")
        
        return Service(
            name=service_name,
            uuid=service_uuid,
            characteristics=characteristics,
            description=description
        )
    
    def extract_characteristics(self, service_body: str, full_content: str) -> List[Characteristic]:
        """Extract characteristics from service definition."""
        characteristics = []
        
        # Pattern to match BT_GATT_CHARACTERISTIC_SIMPLE
        char_pattern = r'BT_GATT_CHARACTERISTIC_SIMPLE\s*\(\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\s*\)'
        
        for match in re.finditer(char_pattern, service_body):
            uuid_ref = match.group(1).strip()
            properties = match.group(2).strip()
            permissions = match.group(3).strip()
            read_func = match.group(4).strip()
            write_func = match.group(5).strip()
            user_data = match.group(6).strip()
            read_packet = match.group(7).strip()
            write_packet = match.group(8).strip()
            
            # Resolve UUID reference to actual value
            uuid = self.resolve_uuid(uuid_ref)
            
            # Parse properties and permissions
            props = self.parse_ble_flags(properties)
            perms = self.parse_ble_flags(permissions)
            
            # Clean up packet types
            read_packet = read_packet if read_packet != 'void' else None
            write_packet = write_packet if write_packet != 'void' else None
            
            # Generate characteristic name from function name
            char_name = write_func if write_func != 'NULL' else read_func
            char_name = char_name.replace('_write', '').replace('_read', '').replace('_', ' ').title()
            
            characteristic = Characteristic(
                name=char_name,
                uuid=uuid,
                properties=props,
                permissions=perms,
                read_packet=read_packet,
                write_packet=write_packet,
                description=f"Characteristic for {char_name.lower()} operations"
            )
            
            characteristics.append(characteristic)
        
        return characteristics
    
    def parse_ble_flags(self, flags_str: str) -> List[str]:
        """Parse BLE flags like BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE."""
        flags = []
        flag_parts = flags_str.split('|')
        
        for part in flag_parts:
            part = part.strip()
            if 'READ' in part:
                flags.append('READ')
            elif 'WRITE_WITHOUT_RESP' in part:
                flags.append('WRITE_WITHOUT_RESP')
            elif 'WRITE' in part:
                flags.append('WRITE')
            elif 'NOTIFY' in part:
                flags.append('NOTIFY')
            elif 'INDICATE' in part:
                flags.append('INDICATE')
        
        return flags
    
    def extract_comment_above(self, content: str, position: int) -> str:
        """Extract comment block above a given position."""
        lines = content[:position].split('\n')
        comment_lines = []
        
        # Look backwards for comment block
        for line in reversed(lines[-10:]):  # Check last 10 lines
            line = line.strip()
            if line.startswith('//'):
                comment_lines.insert(0, line[2:].strip())
            elif line.startswith('/*') or line.endswith('*/'):
                continue  # Skip block comment markers
            elif line.startswith('*'):
                comment_lines.insert(0, line[1:].strip())
            elif not line:
                continue  # Skip empty lines
            else:
                break  # Stop at non-comment line
        
        return ' '.join(comment_lines) if comment_lines else ""

class BLEDocumentationGenerator:
    def __init__(self, parser: BLEProtocolParser):
        self.parser = parser
    
    def generate_markdown_docs(self, output_file: str):
        """Generate comprehensive Markdown documentation."""
        with open(output_file, 'w') as f:
            f.write(self.generate_header())
            f.write(self.generate_overview())
            f.write(self.generate_packet_structures())
            f.write(self.generate_services_documentation())
            f.write(self.generate_usage_examples())
    
    def generate_header(self) -> str:
        return """# nRF5340 BLE Protocol Documentation

**Auto-generated from source code analysis**

This document describes the complete BLE (Bluetooth Low Energy) protocol implementation for the nRF5340 microcontroller project. The protocol uses typed packet structures for type-safe communication.

---

"""
    
    def generate_overview(self) -> str:
        overview = """## Protocol Overview

### Architecture
- **Type-Safe Protocol**: All packets use C struct definitions with explicit byte layouts
- **Industry-Standard Services**: Implements standard Bluetooth SIG services where applicable
- **Custom Services**: Application-specific services for device control and data transfer
- **Error Handling**: Proper BLE error codes and validation

### Services Summary
"""
        
        for service in self.parser.services:
            overview += f"- **{service.name}** ({service.uuid}): {service.description}\n"
        
        overview += "\n---\n\n"
        return overview
    
    def generate_packet_structures(self) -> str:
        docs = """## Packet Structures

All BLE packets use packed C structures for consistent byte layout across platforms.

"""
        
        for struct_name, struct in self.parser.packet_structs.items():
            docs += f"### {struct_name}\n\n"
            if struct.description:
                docs += f"*{struct.description}*\n\n"
            
            docs += f"**Total Size**: {struct.total_size} bytes\n\n"
            
            # Create byte layout table
            docs += "| Offset | Size | Field | Type | Description |\n"
            docs += "|--------|------|-------|------|-------------|\n"
            
            for field in struct.fields:
                offset_str = f"{field.offset}"
                if field.size > 1:
                    offset_str += f"-{field.offset + field.size - 1}"
                
                docs += f"| {offset_str} | {field.size} | `{field.name}` | `{field.type}` | {field.description or 'Data field'} |\n"
            
            # Add hex layout visualization
            docs += f"\n**Hex Layout** ({struct.total_size} bytes):\n```\n"
            hex_layout = ""
            for i in range(struct.total_size):
                if i % 16 == 0:
                    hex_layout += f"{i:04X}: "
                
                # Find which field this byte belongs to
                field_name = "??"
                for field in struct.fields:
                    if field.offset <= i < field.offset + field.size:
                        field_name = field.name[:2].upper()
                        break
                
                hex_layout += f"{field_name} "
                
                if (i + 1) % 16 == 0 or i == struct.total_size - 1:
                    hex_layout += "\n"
            
            docs += hex_layout + "```\n\n"
        
        docs += "---\n\n"
        return docs
    
    def generate_services_documentation(self) -> str:
        docs = """## BLE Services

"""
        
        for service in self.parser.services:
            docs += f"### {service.name}\n\n"
            docs += f"**UUID**: `{service.uuid}`  \n"
            docs += f"**Description**: {service.description}\n\n"
            
            docs += "#### Characteristics\n\n"
            
            for char in service.characteristics:
                docs += f"##### {char.name}\n\n"
                docs += f"- **UUID**: `{char.uuid}`\n"
                docs += f"- **Properties**: {', '.join(char.properties)}\n" 
                docs += f"- **Permissions**: {', '.join(char.permissions)}\n"
                
                if char.write_packet:
                    docs += f"- **Write Packet**: `{char.write_packet}`\n"
                if char.read_packet:
                    docs += f"- **Read Packet**: `{char.read_packet}`\n"
                
                docs += f"- **Description**: {char.description}\n\n"
                
                # Add packet format details
                if char.write_packet and char.write_packet in self.parser.packet_structs:
                    packet = self.parser.packet_structs[char.write_packet]
                    docs += f"**Write Data Format** ({packet.total_size} bytes):\n"
                    for field in packet.fields:
                        docs += f"- Byte {field.offset}: `{field.name}` ({field.type})\n"
                    docs += "\n"
                
                if char.read_packet and char.read_packet in self.parser.packet_structs:
                    packet = self.parser.packet_structs[char.read_packet]
                    docs += f"**Read Data Format** ({packet.total_size} bytes):\n"
                    for field in packet.fields:
                        docs += f"- Byte {field.offset}: `{field.name}` ({field.type})\n"
                    docs += "\n"
        
        docs += "---\n\n"
        return docs
    
    def generate_usage_examples(self) -> str:
        return """## Usage Examples

### Control Service Command

```c
// Send GET_STATUS command
control_command_packet_t cmd = {
    .cmd_id = CMD_GET_STATUS,
    .param1 = 0x00,
    .param2 = 0x00
};
// Write to Control Command characteristic
```

### Data Service Upload

```c
// Upload data chunk
data_upload_packet_t data = {
    .data = {0x01, 0x02, 0x03, ...}  // Up to 20 bytes
};
// Write to Data Upload characteristic
```

### Device Info Service

```c
// Read manufacturer name
device_info_string_t info;
// Read from Manufacturer Name characteristic
// info.text contains null-terminated string
```

### DFU Service

```c
// Start DFU process
dfu_control_packet_t dfu_cmd = {
    .command = DFU_CMD_START_DFU,
    .param = {0x00, ...}  // Parameters
};
// Write to DFU Control Point characteristic

// Send firmware data
dfu_packet_t fw_data = {
    .data = {/* firmware bytes */}
};
// Write to DFU Packet characteristic
```

---

*Documentation generated automatically from source code analysis*
"""

def main():
    """Main function to generate BLE protocol documentation."""
    services_dir = "src/services"
    output_file = "BLE_PROTOCOL_DOCUMENTATION.md"
    
    print("🔍 Parsing BLE service files...")
    parser = BLEProtocolParser(services_dir)
    parser.parse_all_services()
    
    print(f"📊 Found {len(parser.services)} services and {len(parser.packet_structs)} packet structures")
    
    print("📝 Generating documentation...")
    generator = BLEDocumentationGenerator(parser)
    generator.generate_markdown_docs(output_file)
    
    print(f"✅ Documentation generated: {output_file}")
    
    # Print summary
    print("\n📋 Summary:")
    for service in parser.services:
        print(f"  • {service.name}: {len(service.characteristics)} characteristics")
    
    for struct_name, struct in parser.packet_structs.items():
        print(f"  • {struct_name}: {struct.total_size} bytes, {len(struct.fields)} fields")

if __name__ == "__main__":
    main()
