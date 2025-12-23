#!/usr/bin/env python3
"""
Convert SPIR-V binary file to C++ header file
Usage: spv_to_header.py <input.spv> <output.h> <variable_name>
"""

import sys
import os

def spv_to_header(spv_file, header_file, var_name):
    """Convert SPIR-V binary to C++ header with byte array"""

    # Read SPIR-V binary
    with open(spv_file, 'rb') as f:
        spv_data = f.read()

    # Generate header guard
    guard_name = f"SHADER_{var_name.upper()}_H"

    # Write header file
    with open(header_file, 'w') as f:
        f.write(f"// Auto-generated from {os.path.basename(spv_file)}\n")
        f.write(f"// DO NOT EDIT!\n\n")
        f.write(f"#ifndef {guard_name}\n")
        f.write(f"#define {guard_name}\n\n")
        f.write(f"#include <cstdint>\n")
        f.write(f"#include <cstddef>\n\n")

        # Write array declaration
        f.write(f"namespace Shaders {{\n\n")
        f.write(f"constexpr size_t {var_name}_size = {len(spv_data)};\n\n")
        f.write(f"constexpr uint8_t {var_name}_data[] = {{\n")

        # Write bytes in rows of 12
        for i in range(0, len(spv_data), 12):
            chunk = spv_data[i:i+12]
            hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f"    {hex_values},\n")

        f.write(f"}};\n\n")
        f.write(f"}} // namespace Shaders\n\n")
        f.write(f"#endif // {guard_name}\n")

    print(f"Generated {header_file} ({len(spv_data)} bytes)")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: spv_to_header.py <input.spv> <output.h> <variable_name>")
        sys.exit(1)

    spv_file = sys.argv[1]
    header_file = sys.argv[2]
    var_name = sys.argv[3]

    if not os.path.exists(spv_file):
        print(f"Error: {spv_file} not found")
        sys.exit(1)

    spv_to_header(spv_file, header_file, var_name)
