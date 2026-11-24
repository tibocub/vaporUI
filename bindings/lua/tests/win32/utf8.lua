-- non-ASCII: output bytes and a UTF-8 file name through vapor.fs
print("OUT é€")
local d = (os.getenv("TEMP") or ".") .. "/vlr-utf8-" .. os.time()
assert(vapor.fs.mkdir(d))
local f = assert(io.open(d .. "/x.txt", "w")); f:close() -- plain ASCII name via stdio
local ok = vapor.fs.mkdir(d .. "/é€")                      -- UTF-8 name through the wide API
assert(ok, "mkdir with a UTF-8 name")
local names = vapor.fs.list(d)
table.sort(names)
print("NAMES " .. table.concat(names, ","))
assert(names[2] == "é€" and vapor.fs.stat(d .. "/é€").type == "dir")
assert(vapor.fs.rmdir(d .. "/é€")); os.remove(d .. "/x.txt"); assert(vapor.fs.rmdir(d))
print("UTF8 OK")
