local native = require 'sqlite.native'

local sqlite = {
	open = native.open
}

for k,v in pairs(native) do
	sqlite[k] = v
end

native.db.exec_impl = function(self,query,process_row)
	local stm,err = self:prepare(query)
	if not stm then
		return nil,err
	end
	while true do
		local r,err = stm:step()
		if not r then
			return nil,err
		end
		if r == native.DONE then
			break
		end
		if r == native.ROW then
			process_row(stm)
		end
	end
	return stm
end

local function get_row(stm)
	local cnt = stm:column_count()
	local res = {}
	for i=1,cnt do
		local val
		local t = stm:column_type(i-1)
		if t == native.INTEGER then
			val = stm:column_int(i-1)
		elseif t == native.TEXT then
			val = stm:column_text(i-1)
		elseif t == native.FLOAT then
			val = stm:column_double(i-1)
		elseif t == native.BLOB then
			val = stm:column_blob(i-1)
		elseif t == native.NULL then
			val = nil
		end
		res[i] = val
	end
	return res
end

local function get_row_table(stm,names)
	local cnt = stm:column_count()
	if cnt > #names then
		cnt = #names
	end
	local res = {}
	for i=1,cnt do
		local val
		local t = stm:column_type(i-1)
		if t == native.INTEGER then
			val = stm:column_int(i-1)
		elseif t == native.TEXT then
			val = stm:column_text(i-1)
		elseif t == native.FLOAT then
			val = stm:column_double(i-1)
		elseif t == native.BLOB then
			val = stm:column_blob(i-1)
		elseif t == native.NULL then
			val = nil
		end
		res[names[i]] = val
	end
	return res
end

local function process_result(result)
	if not result then
		return true
	end
	return result
end


native.db.exec = function(self,query)
	local result
	local res,err = self:exec_impl(query,function(stm)
		if not result then
			result = {get_row(stm)}
		else
			table.insert(result,get_row(stm))
		end
	end)
	if not res then
		return nil,err
	end
	res:finalize()
	return process_result(result)
end

native.db.exec_names = function(self,query,names)
	local result
	local res,err = self:exec_impl(query,function(stm)
		if not result then
			result = {get_row_table(stm,names)}
		else
			table.insert(result,get_row_table(stm,names))
		end
	end)
	if not res then
		return nil,err
	end
	res:finalize()
	return process_result(result)
end

return sqlite