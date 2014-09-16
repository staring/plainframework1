if _VERSION == "Lua 5.2" then
  xml = require("LuaXML_lib")
elseif _VERSION == "Lua 5.1" then
  xml = require("LuaXML51_lib")
end

if not xml then
  print("error when load xml lib")
  os.exit(1)
end

local base = _G

-- symbolic name for tag index, this allows accessing the tag by var[xml.TAG]
TAG = 0

-- sets or returns tag of a LuaXML object
function xml.tag(var,tag)
	if base.type(var)~="table" then return end
	if base.type(tag)=="nil" then 
		return var[TAG]
	end
	var[TAG] = tag
end

-- creates a new LuaXML object either by setting the metatable of an existing Lua table or by setting its tag
function xml.new(arg)
	if base.type(arg)=="table" then 
		base.setmetatable(arg,{__index=xml, __tostring=xml.str})
		return arg
	end
	local var={}
	base.setmetatable(var,{__index=xml, __tostring=xml.str})
	if base.type(arg)=="string" then var[TAG]=arg end
	return var
end

-- appends a new subordinate LuaXML object to an existing one, optionally sets tag
function xml.append(var,tag)
	if base.type(var)~="table" then return end
	local newVar = xml.new(tag)
	var[#var+1] = newVar
	return newVar
end

-- converts any Lua var into an XML string
function xml.str(var,indent,tagValue)
	if base.type(var)=="nil" then return end
	local indent = indent or 0
	local indentStr=""
	for i = 1,indent do indentStr=indentStr.."	" end
	local tableStr=""

	if base.type(var)=="table" then
		local tag = var[0] or tagValue or base.type(var)
		local s = indentStr.."<"..tag
		for k,v in base.pairs(var) do -- attributes 
			if base.type(k)=="string" then
				if base.type(v)=="table" and k~="_M" then -- otherwise recursiveness imminent
					tableStr = tableStr..xml.str(v,indent+1,k)
				else
					s = s.." "..k.."=\""..xml.encode(base.tostring(v)).."\""
				end
			end
		end
		if #var==0 and #tableStr==0 then
			s = s.." />\n"
		elseif #var==1 and base.type(var[1])~="table" and #tableStr==0 then -- single element
			s = s..">"..xml.encode(base.tostring(var[1])).."</"..tag..">\n"
		else
			s = s..">\n"
			for k,v in base.ipairs(var) do -- elements
				if base.type(v)=="string" then
					s = s..indentStr.."	"..xml.encode(v).." \n"
				else
					s = s..xml.str(v,indent+1)
				end
			end
			s=s..tableStr..indentStr.."</"..tag..">\n"
		end
		return s
	else
		local tag = base.type(var)
		return indentStr.."<"..tag.."> "..xml.encode(base.tostring(var)).." </"..tag..">\n"
	end
end


-- saves a Lua var as xml file
function xml.save(var,filename)
	if not var then return end
	if not filename or #filename==0 then return end
	local file = base.io.open(filename,"w")
	file:write("<?xml version=\"1.0\"?>\n<!-- file \"",filename, "\", generated by LuaXML -->\n\n")
	file:write(xml.str(var))
	base.io.close(file)
end


-- recursively parses a Lua table for a substatement fitting to the provided tag and attribute
function xml.find(var, tag, attributeKey,attributeValue)
	-- check input:
	if base.type(var)~="table" then return end
	if base.type(tag)=="string" and #tag==0 then tag=nil end
	if base.type(attributeKey)~="string" or #attributeKey==0 then attributeKey=nil end
	if base.type(attributeValue)=="string" and #attributeValue==0 then attributeValue=nil end
	-- compare this table:
	if tag~=nil then
		if var[0]==tag and ( attributeValue == nil or var[attributeKey]==attributeValue ) then
			base.setmetatable(var,{__index=xml, __tostring=xml.str})
			return var
		end
	else
		if attributeValue == nil or var[attributeKey]==attributeValue then
			base.setmetatable(var,{__index=xml, __tostring=xml.str})
			return var
		end
	end
	-- recursively parse subtags:
	for k,v in base.ipairs(var) do
		if base.type(v)=="table" then
			local ret = xml.find(v, tag, attributeKey,attributeValue)
			if ret ~= nil then return ret end
		end
	end
end

return xml
