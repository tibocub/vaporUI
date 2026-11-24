-- after vapor.input(false) the console must be cooked again: line input with echo
vapor.input(true)
repeat local ev = {vapor.pull(5)} until ev[1] == "char" and ev[2] == "q" or ev[1] == nil
vapor.input(false)
print("READY")
print("GOT:" .. tostring(io.read("l")))
