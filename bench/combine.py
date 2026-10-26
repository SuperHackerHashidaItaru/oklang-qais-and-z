import os

EXT = ".md"
SRC = "./results/"

def combine_text_files(target_directory, output_file_path):
    """
    Recursively scans target_directory for .txt files and combines them 
    into output_file_path with file name headers.
    """
    # Convert paths to absolute paths to prevent reading the output file if it's placed inside the same directory
    abs_output_path = os.path.abspath(output_file_path)
    
    print(f"Scanning '{target_directory}' for text files...")
    
    # Open the output file in write mode with UTF-8 encoding
    with open(abs_output_path, 'w', encoding='utf-8') as outfile:
        file_count = 0
        
        # os.walk automatically traverses directories recursively
        for root, dirs, files in os.walk(target_directory):
            for file in files:
                # Target text files (case-insensitive check)
                if file.lower().endswith(EXT):
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
                        
    print(f"\nDone! Successfully combined {file_count} text files into '{output_file_path}'.")

if __name__ == "__main__":
    OUTPUT_FILE = f"combined_output{EXT}"
    
    combine_text_files(SRC, OUTPUT_FILE)

