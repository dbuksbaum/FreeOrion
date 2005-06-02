import os, sys, re
import strtable, techs

class Application :

	str_table_re = re.compile(r'.+_stringtable.txt')

	def __init__( self ) :
		self.dataset_folder = None
		self.supported_languages =dict() 

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
					language, table = strtable.load_string_table(self.dataset_folder,entry)			
					self.supported_languages[language] = table
					print "\t",len(table),"entries found for language",language
				except RuntimeError, e :
					print >> sys.stderr, "\tError processing", entry, "at", self.dataset_folder
					print >> sys.stderr, "\t", e

	def __load_techs(self) :
		print "Loading techs"
		self.tech_set = techs.load_techs_descriptions( self.dataset_folder, 'techs.xml' )

instance = Application()
