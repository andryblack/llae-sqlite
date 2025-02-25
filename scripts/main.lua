local log = require 'llae.log'
local async = require 'llae.async'
local fs = require 'llae.fs'
local faker = require 'faker'

local sqlite = require 'db.sqlite'

local db_filename = 'build/test.sqlite'

local encode_string = function(str)
	return "'" .. str:gsub("'","''") .. "'"
end

async.run(function()
	--fs.unlink(db_filename)

	local st = os.time()

	local sql = assert(sqlite.open(db_filename))
	assert(sql:exec('CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name VARCHAR(128));'))

	local f = faker.faker

	while true do
		local res = assert(sql:exec_names('SELECT count(*) FROM test;',{'count'}))
		local count = res[1].count
		if count > 50000 then
			log.info('count:',count)
			break
		else
			log.info('count:',count,'add 1000')
		end
		
		for i=1,1000 do
			local query = "INSERT INTO test (name) VALUES(" .. encode_string(f.person:fullName()) .. ");"
			assert(sql:exec(query))
		end
	end

	local res = assert(sql:exec_names('SELECT * FROM test;',{'id','name'}))
	for _,v in ipairs(res) do
		log.info('row',v.id,v.name)
	end

	log.info('time:',os.difftime(os.time(),st))
end)