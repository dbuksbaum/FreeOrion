import sys, os, re

upper_re = re.compile(r'^[_ABCDEFGHIJKLMNOPQRSTUVWXYZ\d]+$')

def add_entries( string_table, strings ) :
	for key, value in strings.items() :
		string_table[key] = value 

def remove_entries( string_table, strings ) :
	for string in strings :
		try:
			del string_table[string]
		except KeyError :
			print >> sys.stderr, "Could not remove", string, "because it is not in the string table!"

def store_string_table( base, name, language, table ) :
	filename = base + '/' + name
	
	table_stream = open( filename, 'w' )

	print >> table_stream, language
	print >> table_stream	

	for key, value in table.items() :
		print >> table_stream, key
		print >> table_stream, value
		print >> table_stream

	table_stream.close()
	

def load_string_table( base, name ) :
	path = base + '/' + name
	
	table_stream = open( path )
	
	string_lines = []


	for line in table_stream :
		line_text = line.strip()
		if len(line_text)>0 and line_text[0] != "#" :
			string_lines.append(line_text)
	
	table = dict()
	language = string_lines[0]
	i = 1

	current_key = None
	current_text = []
	
	while i < len(string_lines)	:
		match = upper_re.search(string_lines[i])
		if match != None :
			# new key found
			if current_key is None :
				current_key = string_lines[i]
			else :
				table[current_key] = '\n'.join(current_text)
				current_key = string_lines[i]
				current_text = []
		else :
			current_text.append(string_lines[i])
		i+=1

	table_stream.close()

	return language, table
