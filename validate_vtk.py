#!/usr/bin/env python3
"""
Simple VTK file validation script to check for common format errors
"""
import sys
import os

def validate_vtk_file(filepath):
    """Validate a VTK legacy file format"""
    errors = []
    warnings = []
    
    if not os.path.exists(filepath):
        return [f"File not found: {filepath}"], []
    
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    # Check header
    if not lines[0].startswith("# vtk DataFile Version"):
        errors.append("Missing or invalid VTK version header")
    
    # Find CELL_DATA section
    cell_data_idx = -1
    for i, line in enumerate(lines):
        if line.startswith("CELL_DATA"):
            cell_data_idx = i
            break
    
    if cell_data_idx >= 0:
        # Check for comments in cell data section
        for i in range(cell_data_idx, len(lines)):
            line = lines[i].strip()
            if line.startswith("#") and not lines[i-1].strip().startswith("LOOKUP_TABLE"):
                errors.append(f"Invalid comment in CELL_DATA section at line {i+1}: {line[:50]}")
    
    # Check for required sections
    required_sections = ["ASCII", "DATASET", "POINTS", "CELLS", "CELL_TYPES"]
    found_sections = []
    
    for line in lines[:20]:  # Check first 20 lines for sections
        for section in required_sections:
            if section in line:
                found_sections.append(section)
    
    for section in required_sections:
        if section not in found_sections:
            warnings.append(f"Section '{section}' not found in first 20 lines")
    
    return errors, warnings

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 validate_vtk.py <vtk_file> [vtk_file2] ...")
        sys.exit(1)
    
    total_errors = 0
    
    for filepath in sys.argv[1:]:
        print(f"\nValidating: {filepath}")
        print("-" * (12 + len(filepath)))
        
        errors, warnings = validate_vtk_file(filepath)
        
        if errors:
            print(f"❌ ERRORS ({len(errors)}):")
            for error in errors:
                print(f"   • {error}")
            total_errors += len(errors)
        else:
            print("✅ No errors found")
        
        if warnings:
            print(f"⚠️  WARNINGS ({len(warnings)}):")
            for warning in warnings:
                print(f"   • {warning}")
        
        print(f"Status: {'INVALID' if errors else 'VALID'}")
    
    print(f"\n{'='*50}")
    print(f"Total files validated: {len(sys.argv[1:])}")
    print(f"Total errors: {total_errors}")
    print(f"Overall result: {'FAILED' if total_errors > 0 else 'PASSED'}")
    
    return 1 if total_errors > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
