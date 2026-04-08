--!strict
-- Helpers for behavior `properties` schemas (`defineProperties`, `prop.*`).
-- Consumed by the engine at behavior load time; also available to behaviors via globals.

local PROP_MARK = "__behaviorPropDesc"

local function inferPlain(v: any): any
	if v == nil then
		error('defineProperties: nil default cannot be inferred (use prop.object() etc.)')
	end
	local tv = type(v)
	if tv == "number" then
		return { [PROP_MARK] = true, type = "number", default = v }
	elseif tv == "string" then
		return { [PROP_MARK] = true, type = "string", default = v }
	elseif tv == "boolean" then
		return { [PROP_MARK] = true, type = "boolean", default = v }
	else
		error(`defineProperties: cannot infer property type from {tv}`)
	end
end

local function normalizeEntry(name: string, v: any): any
	if type(v) ~= "table" or v[PROP_MARK] ~= true then
		v = inferPlain(v)
	end
	local desc: any = v
	if desc.type == nil then
		error(`defineProperties: invalid descriptor for {name}`)
	end
	if desc.default == nil and desc.type ~= "object" then
		error(`defineProperties: missing default for {name}`)
	end
	if desc.type == "enum" then
		if type(desc.default) ~= "string" then
			error(`defineProperties: enum {name} needs string default`)
		end
	end
	if desc.type == "enum" then
		local opts = desc.enumOptions
		if type(opts) ~= "table" or #opts == 0 then
			error(`defineProperties: enum {name} needs non-empty enumOptions`)
		end
		local ok = false
		for _, o in ipairs(opts) do
			if o == desc.default then
				ok = true
				break
			end
		end
		if not ok then
			error(`defineProperties: enum default for {name} must be one of enumOptions`)
		end
	end
	desc.name = name
	return desc
end

local function defineProperties(t: { [string]: any }): any
	if type(t) ~= "table" then
		error("defineProperties: expected table")
	end
	local fields: { [string]: any } = {}
	for name, v in pairs(t :: any) do
		if type(name) ~= "string" then
			error("defineProperties: property names must be strings")
		end
		if not string.match(name, "^[%a_][%w_]*$") then
			error(`defineProperties: illegal property name "{name}"`)
		end
		if fields[name] ~= nil then
			error(`defineProperties: duplicate property "{name}"`)
		end
		fields[name] = normalizeEntry(name, v)
	end
	return {
		__kind = "BehaviorProperties",
		fields = fields,
	}
end

local prop: any = {}

function prop.number(default: number, meta: { min: number?, max: number?, slider: boolean? }?): any
	local d: any = { [PROP_MARK] = true, type = "number", default = default }
	if meta then
		if meta.min ~= nil then
			d.min = meta.min
		end
		if meta.max ~= nil then
			d.max = meta.max
		end
		if meta.slider then
			if meta.min == nil or meta.max == nil then
				error("prop.number: slider requires min and max")
			end
			d.slider = true
		end
	end
	return d
end

function prop.integer(default: number, meta: { min: number?, max: number?, slider: boolean? }?): any
	local d: any = { [PROP_MARK] = true, type = "integer", default = math.floor(default) }
	if meta then
		if meta.min ~= nil then
			d.min = meta.min
		end
		if meta.max ~= nil then
			d.max = meta.max
		end
		if meta.slider then
			if meta.min == nil or meta.max == nil then
				error("prop.integer: slider requires min and max")
			end
			d.slider = true
		end
	end
	return d
end

function prop.string(default: string): any
	return { [PROP_MARK] = true, type = "string", default = default }
end

function prop.boolean(default: boolean): any
	return { [PROP_MARK] = true, type = "boolean", default = default }
end

function prop.enum(defaultVal: string, options: { string }): any
	return { [PROP_MARK] = true, type = "enum", default = defaultVal, enumOptions = options }
end

function prop.object(defaultVal: any?): any
	return { [PROP_MARK] = true, type = "object", default = defaultVal }
end

function prop.asset(defaultPath: string?): any
	return { [PROP_MARK] = true, type = "asset", default = defaultPath or "" }
end

function prop.color(defaultR: number, defaultG: number, defaultB: number, defaultA: number?): any
	return {
		[PROP_MARK] = true,
		type = "color",
		default = { defaultR, defaultG, defaultB, defaultA or 255 },
	}
end

function prop.vector(defaultX: number, defaultY: number): any
	return { [PROP_MARK] = true, type = "vector", default = { defaultX, defaultY } }
end

return {
	defineProperties = defineProperties,
	prop = prop,
}
