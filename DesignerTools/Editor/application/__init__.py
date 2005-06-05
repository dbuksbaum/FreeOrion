import os, sys, re
import strtable, techs

class Application :

	str_table_re = re.compile(r'(.+)_stringtable.txt')

	def __init__( self ) :
		self.dataset_folder = None
		self.supported_languages = dict()
		self.language_prefixes = dict()
		self.techs_modified = False
		self.strings_modified = False 

	def shutdown( self ) :
		if self.strings_modified :
			self.__store_strings()
		if self.techs_modified :
			self.__store_techs()

	def __store_strings( self ) :
		print "Storing string tables..."
		strtable.store_string_table( self.dataset_folder, 
									self.language_prefixes['English']+'_stringtable.txt',
									'English',
									self.supported_languages['English'] )
		
		

	def __store_techs( self ) :
		print "Storing techs..."
		self.tech_set.store_techs( self.dataset_folder, 'techs.xml' )

	def load_dataset( self ) :
		print "Loading game dataset from", self.dataset_folder
		self.__load_string_tables()
		self.__load_techs()

	def __load_string_tables(self) :
		print "Loading string tables"
		for entry in os.listdir(self.dataset_folder) :
			match = self.str_table_re.search(entry)
			if match != None :
				try:
					prefix = match.groups()[0]
					language, table = strtable.load_string_table(self.dataset_folder,entry)			
					self.supported_languages[language] = table
					self.language_prefixes[language] = prefix
					print "\t",len(table),"entries found for language",language
				except RuntimeError, e :
					print >> sys.stderr, "\tError processing", entry, "at", self.dataset_folder
					print >> sys.stderr, "\t", e

	def __load_techs(self) :
		print "Loading techs"
		self.tech_set = techs.load_techs_descriptions( self.dataset_folder, 'techs.xml' )

instance = Application()
