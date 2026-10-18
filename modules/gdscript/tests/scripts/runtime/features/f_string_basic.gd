extends Node

func f() -> int:
	return 5

func test():
	var x := 1
	var label_name := "q"
	var f_var := "id"
	print(f"a{x}b")
	print(f"{{braced}} {x}")
	print(f"{x + 2}")
	print(f"{label_name.to_upper()}!")
	print(f_var)
	print(f())
