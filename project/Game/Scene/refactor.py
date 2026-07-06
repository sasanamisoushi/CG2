import re

def extract_function(file_content, func_name_pattern):
    # Find the start of the function
    match = re.search(r'^[\w\s\*&]*\s+GamePlayScene::' + func_name_pattern + r'\s*\([^)]*\)\s*(const)?\s*\{', file_content, re.MULTILINE)
    if not match:
        return None, file_content
    
    start_index = match.start()
    
    # Find the end of the function by counting braces
    brace_count = 0
    in_string = False
    in_char = False
    in_comment = False
    in_line_comment = False
    
    end_index = -1
    
    for i in range(match.end() - 1, len(file_content)):
        char = file_content[i]
        next_char = file_content[i+1] if i+1 < len(file_content) else ''
        prev_char = file_content[i-1] if i-1 >= 0 else ''
        
        if in_line_comment:
            if char == '\n':
                in_line_comment = False
            continue
            
        if in_comment:
            if char == '*' and next_char == '/':
                in_comment = False
            continue
            
        if in_string:
            if char == '"' and prev_char != '\\':
                in_string = False
            continue
            
        if in_char:
            if char == "'" and prev_char != '\\':
                in_char = False
            continue
            
        if char == '/' and next_char == '/':
            in_line_comment = True
            continue
            
        if char == '/' and next_char == '*':
            in_comment = True
            continue
            
        if char == '"':
            in_string = True
            continue
            
        if char == "'":
            in_char = True
            continue
            
        if char == '{':
            brace_count += 1
        elif char == '}':
            brace_count -= 1
            if brace_count == 0:
                end_index = i + 1
                break
                
    if end_index != -1:
        extracted = file_content[start_index:end_index]
        new_content = file_content[:start_index] + file_content[end_index:]
        return extracted, new_content
        
    return None, file_content

def main():
    path = "GamePlayScene.cpp"
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
        
    funcs_to_extract = [
        "UpdateGameplayCamera",
        "UpdateCinematicLockOnCamera",
        "SpawnEnemiesFromSpawnPoints",
        "SpawnEnemyFromSpawnPoint",
        "IsEnemySpawnPointActive",
        "ScheduleEnemySpawn",
        "TriggerEnemyReinforcements",
        "UpdateEnemyRespawns"
    ]
    
    extracted_funcs = {}
    
    for func in funcs_to_extract:
        ext, content = extract_function(content, func)
        if ext:
            extracted_funcs[func] = ext
            print(f"Extracted {func}")
        else:
            print(f"Failed to extract {func}")
            
    with open("GamePlayScene_refactored.cpp", "w", encoding="utf-8") as f:
        f.write(content)
        
    with open("extracted_funcs.txt", "w", encoding="utf-8") as f:
        for k, v in extracted_funcs.items():
            f.write(f"// --- {k} ---\n{v}\n\n")

if __name__ == "__main__":
    main()
