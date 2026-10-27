import os
import argparse

def combine_text_files(target_directory, output_file_path, extension):
    """
    Recursively scans target_directory for files matching the given extension 
    and combines them into output_file_path with file name headers.
    """
    # Normalize extension to ensure it starts with a dot
    if not extension.startswith('.'):
        extension = f".{extension}"
        
    extension = extension.lower()
    abs_output_path = os.path.abspath(output_file_path)
    
    print(f"Scanning '{target_directory}' for '{extension}' files...")
    
    # Open the output file in write mode with UTF-8 encoding
    with open(abs_output_path, 'w', encoding='utf-8') as outfile:
        file_count = 0
        
        # os.walk automatically traverses directories recursively
        for root, dirs, files in os.walk(target_directory):
            for file in files:
                # Target files (case-insensitive check)
                if file.lower().endswith(extension):
                    full_path = os.path.join(root, file)
                    
                    # Skip the output file if it happens to be in the scan path
                    if os.path.abspath(full_path) == abs_output_path:
                        continue
                    
                    # Get a clean relative path for the header documentation
                    rel_path = os.path.relpath(full_path, target_directory)
                    print(f"Processing: {rel_path}")
                    
                    # Write a clear, visible visual header for the file block
                    outfile.write("\n" + "="*80 + "\n")
                    outfile.write(f"FILE: {rel_path}\n")
                    outfile.write("="*80 + "\n\n")
                    
                    # Read the contents and write to the combined file
                    try:
                        with open(full_path, 'r', encoding='utf-8', errors='replace') as infile:
                            outfile.write(infile.read())
                        outfile.write("\n")  # Ensure spacing at the end of the file content
                        file_count += 1
                    except Exception as e:
                        outfile.write(f"[ERROR: Could not read this file. Details: {e}]\n\n")
                        print(f"Error reading {rel_path}: {e}")
                        
    print(f"\nDone! Successfully combined {file_count} files into '{output_file_path}'.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Combine multiple text files into a single document.")
    
    # Define flags with your existing defaults
    parser.add_argument("-e", "--ext", default=".md", help="File extension to search for (e.g., .md, .txt). Default: .md")
    parser.add_argument("-s", "--src", default="./results/", help="Source directory to scan. Default: ./results/")
    parser.add_argument("-o", "--out", default=None, help="Output file path. Default: combined_output<ext>")
    
    args = parser.parse_args()
    
    # If no output path is explicitly provided, name it based on the chosen extension
    if args.out is None:
        clean_ext = args.ext if args.ext.startswith('.') else f".{args.ext}"
        output_file = f"combined_output{clean_ext}"
    else:
        output_file = args.out
        
    combine_text_files(args.src, output_file, args.ext)

