print("write-nil-case")
t = @()
t["missing"] = nil
r,v = next(t,"missing")
if r = nil then
 print("write-nil-next-ok")
else
 print("write-nil-next-bad")
end

print("read-miss-case")
u = @()
x = u["missing"]
print("before-next-read-miss")
r,v = next(u,"missing")
print("after-next-read-miss")
