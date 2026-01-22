name = 'llae-sqlite'


dependencies = {
	'llae',
	'sqlite',
}

cmodules = {
	'sqlite.native'
}

function install() 
 	install_script(dir .. '/scripts/db/sqlite.lua','db/sqlite.lua')
end

includedir = '${dir}/src'

build_lib = {

  	project = [[
		sysincludedirs{
			<%= format_mod_file(project:get_module('llae'),'src')%>,
			'include/llae-private',
			'include',
		}
		files {
			<%= format_file(module.dir,'src','*.cpp')  %>,
			<%= format_file(module.dir,'src','*.h')  %>,
		}
	]]
}