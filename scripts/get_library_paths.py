
def get_library_paths():
    try:
        from epicscorelibs.path import lib_path
        print(lib_path)
    except ImportError:
        return


if __name__ == "__main__":
    get_library_paths()