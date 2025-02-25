name = 'sqlite'
version = '3.49.1'
versioncode = '3490100'
archive = 'sqlite-autoconf-' .. versioncode .. '.tar.gz'
url = 'https://www.sqlite.org/2025/' .. archive
hash = '8d77d0779bcd9993eaef33431e2e0c30'

dir = name .. '-' .. version

project_config = {
	--{'apicheck',type='boolean'},
}

function install()
	download(url,archive,hash)

	unpack_tgz(archive,dir,1)

	preprocess {
		src = dir..'/sqlite3.h',
		remove_src = true,
		dst = 'build/include/sqlite3.h',
		comment = {	},
		insert_before = {
			--['#ifndef SQLITE_EXTERN'] = '#define SQLITE_EXTERN'
		}
	}

	write_file(location .. '/' .. dir .. '/sqlite3_build_config.h',[=[

/* #define SQLITE_EXTERN */
#define SQLITE_OMIT_JSON
#define SQLITE_OMIT_JSON
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_THREADSAFE 0
/* #define SQLITE_ENABLE_GEOPOLY 0 */
/* #define SQLITE_ENABLE_RTREE 0 */
/* #define SQLITE_ENABLE_FTS4 0 */
/* #define SQLITE_ENABLE_FTS5 0 */

]=])
	
end

build_lib = {
	project = [[
		includedirs{
			'include'
		}
		files {
			<%= format_file(module.dir,'sqlite3.c') %>,
			'include/sqlite3.h'
		}
		defines {
			'SQLITE_CUSTOM_INCLUDE=sqlite3_build_config.h',
			
		}
]]
}

