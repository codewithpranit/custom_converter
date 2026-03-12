import sys
import os
from .converter_wrapper import converter

def main():
    print("========================================")
    print("   Custom Encoding Converter CLI")
    print("========================================")
    
    while True:
        print("\nSelect a conversion function:")
        print("1. pipeline (Standard full flow)")
        print("2. utf8_to_acharya")
        print("3. acharya_to_utf8")
        print("4. acharya_to_iscii")
        print("5. iscii_to_acharya")
        print("6. utf8_to_iscii")
        print("7. iscii_to_utf8")
        print("0. Exit")
        
        choice = input("\nEnter choice (0-7): ").strip()
        
        if choice == '0':
            print("Exiting...")
            break
        
        mapping = {
            '1': ('pipeline', 'file'),
            '2': ('utf8_to_acharya', 'file'),
            '3': ('acharya_to_utf8', 'hex'),
            '4': ('acharya_to_iscii', 'hex'),
            '5': ('iscii_to_acharya', 'hex'),
            '6': ('utf8_to_iscii', 'file'),
            '7': ('iscii_to_utf8', 'hex'),
        }
        
        if choice not in mapping:
            print("Invalid choice. Please try again.")
            continue
            
        cmd, input_type = mapping[choice]
        
        if input_type == 'file':
            arg = input("Enter path to .txt file: ").strip()
            if not os.path.exists(arg):
                print(f"Error: File '{arg}' not found.")
                continue
        else:
            print("Enter hex codes (e.g., '0xFA 0x01 B4 00'):")
            arg = input("> ").strip()
            if not arg:
                print("Error: Hex input cannot be empty.")
                continue
        
        print("\n--- Output ---")
        output = converter.run(cmd, arg)
        print(output)
        print("--------------")

if __name__ == "__main__":
    main()
