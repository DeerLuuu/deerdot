@Author("deer")
extends Node

class Inner:
	@warning_ignore("unused_signal")
	@Mark
	signal ping
	@Tag
	var value := 1
	@Route("/m", method = "POST")
	func method() -> void:
		pass

@A
@B
@A
func dup() -> void:
	pass

func test() -> void:
	var s: GDScript = get_script()
	print(s.has_annotation(&"", &"Author"))
	print(s.get_annotation(&"", &"Author"))
	var all := s.get_annotations(&"dup")
	print(all.size())
	print(all[0]["name"], all[1]["name"], all[2]["name"])
	print(s.get_annotation(&"Inner/method", &"Route"))
	var symbols := s.get_annotation_symbols()
	var names: Array[String] = []
	for symbol: StringName in symbols:
		names.append(String(symbol))
	names.sort()
	print(names)
