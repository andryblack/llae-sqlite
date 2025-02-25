-- project llae-sqlite
project 'llae-sqlite'
-- @modules@
module 'llae'
module 'sqlite'
module 'git://git@github.com:andryblack/llae-faker.git'

cmodule 'sqlite.native'

premake{
	project = [[
	files{
		<%= format_file('src','*.cpp')%>,
		<%= format_file('src','*.h')%>,
	}
	]]
}