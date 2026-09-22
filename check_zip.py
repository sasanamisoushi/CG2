import zipfile
import sys

def main():
    zip_path = 'LE3B_12_ササナミ_ソウシ.zip'
    try:
        with zipfile.ZipFile(zip_path, 'r') as z:
            infos = z.infolist()
            infos.sort(key=lambda x: x.file_size, reverse=True)
            for info in infos[:100]:
                try:
                    name = info.filename.encode('cp437').decode('cp932')
                except Exception:
                    name = info.filename
                
                size_mb = info.file_size / (1024 * 1024)
                print(f"{size_mb:.2f} MB - {name}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
