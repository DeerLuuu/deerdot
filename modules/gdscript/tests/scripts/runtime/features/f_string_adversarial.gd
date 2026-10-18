extends Node

func test():
	var m := {"}": 7}
	print(f"{ m["}"] }")
	print(f"{ '%}' }")
	print(f"{ {'k': 5}['k'] }")
	print(f"{ [1, 2].map(func(v): return v * 2)[0] }")
