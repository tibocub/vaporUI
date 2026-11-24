-- inside vapor.input(true) Ctrl+C is a key event; the app decides
vapor.input(true)
local ev = {vapor.pull(5)}
print("EV " .. table.concat(ev, ","))
vapor.input(false)
print("DONE")
