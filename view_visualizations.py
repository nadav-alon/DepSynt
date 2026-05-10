#!/usr/bin/env python3
import os
import sys
import subprocess

def create_viewer(folder_path):
    if not os.path.isdir(folder_path):
        print(f"Error: Directory '{folder_path}' does not exist.")
        sys.exit(1)

    dot_files = [
        "original_automaton.dot",
        "transducer.dot",
        "combined_automaton.dot"
    ]
    
    svgs = {}
    
    for df in dot_files:
        df_path = os.path.join(folder_path, df)
        if os.path.exists(df_path):
            try:
                # Convert dot to svg
                svg_data = subprocess.check_output(['dot', '-Tsvg', df_path], text=True)
                # Extract just the <svg>...</svg> part, skip the xml header
                if '<svg' in svg_data:
                    svg_data = svg_data[svg_data.find('<svg'):]
                svgs[df] = svg_data
            except Exception as e:
                print(f"Failed to process {df_path}: {e}")
                svgs[df] = f"<p style='color:red;'>Failed to render SVG</p>"
        else:
            svgs[df] = f"<p>File not found</p>"

    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Visualization Viewer - {os.path.basename(os.path.normpath(folder_path))}</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: #f4f4f9;
            color: #333;
            margin: 0;
            padding: 20px;
        }}
        h1 {{
            text-align: center;
            margin-bottom: 20px;
        }}
        .container {{
            display: flex;
            gap: 20px;
            justify-content: center;
            align-items: flex-start;
            flex-wrap: wrap;
        }}
        .panel {{
            background: white;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            padding: 20px;
            flex: 1;
            min-width: 300px;
            display: flex;
            flex-direction: column;
            align-items: center;
            overflow-x: auto;
        }}
        .panel h2 {{
            font-size: 1.2rem;
            margin-top: 0;
            margin-bottom: 15px;
            color: #555;
            text-align: center;
            border-bottom: 2px solid #eaeaea;
            padding-bottom: 10px;
            width: 100%;
        }}
        .panel svg {{
            max-width: 100%;
            height: auto;
        }}
    </style>
</head>
<body>
    <h1>{os.path.basename(os.path.normpath(folder_path))}</h1>
    <div class="container">
        <div class="panel">
            <h2>Original Automaton</h2>
            {svgs.get('original_automaton.dot', '')}
        </div>
        <div class="panel">
            <h2>Transducer</h2>
            {svgs.get('transducer.dot', '')}
        </div>
        <div class="panel">
            <h2>Combined Automaton</h2>
            {svgs.get('combined_automaton.dot', '')}
        </div>
    </div>
</body>
</html>
"""
    output_html = os.path.join(folder_path, "index.html")
    with open(output_html, "w") as f:
        f.write(html_content)
    print(f"Generated viewer at: {output_html}")
    print(f"Open this file in your browser to view the side-by-side visualizations.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 view_visualizations.py <path_to_test_result_folder>")
        print("Example: python3 view_visualizations.py visualizations/E2E_Synthesis_Result")
        sys.exit(1)
        
    create_viewer(sys.argv[1])
