import os, sys

import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade
import gtk.gdk

from tools import not_implemented_feature_dialog, recoverable_error
from prereq_adder import *
from tech_adder import *
from application.techs import Tech
import application

class TechWindowController :
	
	def __init__( self ) :
		self.view = gtk.glade.XML('glade-files/widgets/widgets.glade','tech_window')
		self.window = self.view.get_widget('tech_window')

		# signal connection

		self.view.signal_connect('on_approve_changes_item_activate', self.on_changes_approved )
		self.view.signal_connect('on_discard_changes_item_activate', self.on_changes_discarded )
		self.view.signal_connect('on_find_tech_item_activate', self.on_find_tech )
		self.view.signal_connect('on_language_item_activate', self.on_change_language_display )
		self.view.signal_connect('on_add_prerequisite_clicked', self.__on_add_prerequisite )
		self.view.signal_connect('on_remove_prerequisite_clicked', self.__on_remove_prerequisites )

		self.view.signal_connect('on_tech_cost_button_value_changed', self.__on_tech_cost_changed )
		self.view.signal_connect('on_tech_turns_button_value_changed', self.__on_tech_turns_changed )
		self.view.signal_connect('on_tech_type_combo_box_changed', self.__on_tech_type_changed )
		
		self.view.signal_connect('on_add_tech_button_clicked', self.__on_add_tech )
		self.view.signal_connect('on_remove_tech_button_clicked', self.__on_remove_tech )
	
		self.window.connect('delete_event', self.on_window_deleted )

		# models for tech tree, prerequisite, unlocked items and effects lists
		self.tech_tree = gtk.TreeStore(str,str)
		self.prereq_list = gtk.ListStore(str,str)
		self.items_list = gtk.ListStore(str,str)
		self.effects_list = gtk.ListStore(str,str)

		# treeviews setup
		self.__setup_techs_list_view()
		self.__setup_prerequisites_view()
		self.__setup_unlocked_items_view()
		self.__setup_effects_view()

		# combo-boxes setup
		self.tech_types_list = gtk.ListStore(str)
		self.tech_categories_list = gtk.ListStore(str)
		self.__setup_tech_types_combo_box()
		self.__setup_tech_categories_combo_box()

		self.desc_text_buffer = gtk.TextBuffer()
		self.desc_text_buffer.connect('modified-changed', self.__on_description_changed )
		
		self.current_tech = None
		self.modified_techs = dict()
		self.refreshing = False
		self.techs_marked_for_death = []

	def __on_add_tech( self, button, *args ) :
		
		dialog_controller = AddTechDlgController()

		tech_created = dialog_controller.execute()

		if tech_created : self.__read_tech_list()

	def __handle_current_tech_removal( self ) :
		# Switch display to first tech in tree another tech
		self.current_tech = None
		root_iter = self.tech_tree.get_iter_root()
		first_tech_iter = self.tech_tree.iter_children( root_iter )
		if first_tech_iter is None :
			# clear tech widgets
			self.__clear_widgets()
			self.view.get_widget('techs_list_view').set_cursor((0,))
		else :
			self.__display_tech_at_iter( first_tech_iter, undo_bookkeeping = False )
			self.view.get_widget('techs_list_view').set_cursor((0,0))

	def __on_remove_tech( self, button, *args ) :
		print "Removing a tech..."
		techs_list_view = self.view.get_widget('techs_list_view')
		_, paths = techs_list_view.get_selection().get_selected_rows()
		techs_to_remove = []
		if len(paths) > 0 :
			for path in paths :
				iter = self.tech_tree.get_iter(path)
				tech_id = self.tech_tree.get_value(iter,0)
				techs_to_remove.append(tech_id)
		else :
			return

		strings = application.instance.supported_languages['English']
		tech_listing = []
		for tech_id in techs_to_remove :
			tech_listing.append( "%s %s"%(tech_id, strings[tech_id]) )
		
		message = """You are going to remove the following techs:

%s

Are you sure you wish to remove them?"""%'\n'.join(tech_listing)			

		dialog = gtk.MessageDialog( parent=self.window, type=gtk.MESSAGE_WARNING,buttons=gtk.BUTTONS_YES_NO,
									message_format=message )
		response = dialog.run()
			
		if response == gtk.RESPONSE_YES :
			print "User wants to remove techs..."
			for tech_id in techs_to_remove :
				if tech_id == self.current_tech.name :
					self.__handle_current_tech_removal()
				try :
					self.modified_techs[tech_id]
					del self.modified_techs[tech_id]
				except KeyError :
					pass
			self.techs_marked_for_death += techs_to_remove

			self.__read_tech_list()	
			

		dialog.hide()
		dialog.destroy()

	def __on_tech_type_changed( self, combobox, *args ) :
		if self.current_tech is None or self.refreshing: return
		
		iter = combobox.get_active_iter()

		# This is necessary because there seems to be a bug in
		# PyGtk that gets the changed signal emitted thrice:
		# once with an iter, and twice with a null iter...
		if iter is None : return

		selected_type = self.tech_types_list.get_value(iter,0)
		self.current_tech.tech_type = selected_type


		combobox.child.set_text(selected_type)
		self.current_tech.type = selected_type
		self.current_tech.modified = True
	
		strings = application.instance.supported_languages['English']
		type_str_txt = self.view.get_widget('tech_type_string_entry')
		try:
			type_str_txt.set_text( strings[self.current_tech.type] )
		except KeyError :
			pass

	def __setup_tech_types_combo_box( self ) :
		tech_type_combo = self.view.get_widget( 'tech_type_combo_box' )	
		tech_type_combo.set_model(None)
		cell = gtk.CellRendererText()
		tech_type_combo.pack_start(cell, True)
		tech_type_combo.add_attribute(cell, 'text', 0)
		for type in Tech.types :
			self.tech_types_list.append( (type,) ) 		

		tech_type_combo.set_model(self.tech_types_list)

	def __setup_tech_categories_combo_box( self ) :
		tech_cat_combo = self.view.get_widget( 'tech_category_combo_box' )
		tech_cat_combo.set_model( None )
		cell = gtk.CellRendererText()
		tech_cat_combo.pack_start(cell, True)
		tech_cat_combo.add_attribute(cell, 'text', 0)
		tech_set = application.instance.tech_set
		for cat_identifier in tech_set.tech_categories.keys() :
			self.tech_categories_list.append( (cat_identifier,) )		

		tech_cat_combo.set_model( self.tech_categories_list ) 

	def __on_tech_cost_changed( self, button, *args ) :
		if self.current_tech is None or self.refreshing : return
		self.current_tech.research_cost = button.get_value_as_int()	
		self.current_tech.modified = True
		print self.current_tech.name, "was changed!"
		
	def __on_tech_turns_changed( self, button, *args ) :
		if self.current_tech is None or self.refreshing: return

		self.current_tech.research_turns = button.get_value_as_int()
		self.current_tech.modified = True
		print self.current_tech.name, "was changed!"

	def __on_description_changed( self, textbuffer, *args ) :
		if self.current_tech is None or self.refreshing: return

		strings = application.instance.supported_languages['English']
		strings[self.current_tech.description] = textbuffer.get_text(textbuffer.get_start_iter(), textbuffer.get_end_iter())				

		self.current_tech.modified = True	

	def __on_add_prerequisite( self, button, *args ) :
		if self.current_tech is None : return
		print "Adding prerequisites..."

		dialog = PrerequisitesAdderController()		

		techs_to_add = dialog.execute()

		if len(techs_to_add) == 0 : return
	
		for tech in techs_to_add :
			try:
				self.current_tech.prerequisites.index(tech)
				continue
			except ValueError :
				self.current_tech.prerequisites.append(tech)
				self.current_tech.modified = True

		self.__display_prerequisites()


	def __on_remove_prerequisites( self, button, *args ) :
		if self.current_tech is None : return

		print "Removing a prerequisite"
		prereq_view = self.view.get_widget('prerequisites_view')
		_, paths = prereq_view.get_selection().get_selected_rows()
		techs_to_remove = []
		if len(paths) > 0 :
			for path in paths :
				iter = self.prereq_list.get_iter(path)
				tech_id = self.prereq_list.get_value(iter,0)
				self.current_tech.prerequisites.remove(tech_id)
			
			self.current_tech.modified = True
			self.__display_prerequisites()

	def __setup_techs_list_view( self ) :
		techs_list = self.view.get_widget('techs_list_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		techs_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		techs_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		techs_list.set_search_column(0)
		
		techs_list.connect( 'row-activated', self.__on_tech_activated )

	def __undo_bookkeeping( self ) :
		if self.current_tech is None : return
		if self.current_tech.modified is True :
			self.modified_techs[self.current_tech.name] = self.current_tech
		#print "Techs modified count", len(self.modified_techs.values())

	def __get_tech_for_edit( self, tech, undo_bookkeeping ) :
		#print "Tech", tech.name, "was selected"
		if undo_bookkeeping :
			self.__undo_bookkeeping()
		try :
			self.current_tech = self.modified_techs[tech.name]
		except KeyError :
			self.current_tech = tech.copy()

	def __display_tech_at_iter( self, iter, undo_bookkeeping = True ) :
		tech_id = self.tech_tree.get_value( iter, 0 )
		tech_set = application.instance.tech_set
		try:
			tech = tech_set.tech_entries[tech_id]
			self.__get_tech_for_edit( tech, undo_bookkeeping )
			self.__display_current_tech()			
		except KeyError :
			#print tech_id, "is not a tech"
			self.__clear_widgets()

	def __display_tech_at_path( self, path ) :
		iter = self.tech_tree.get_iter(path)
		self.__display_tech_at_iter( iter )

	def __on_tech_activated( self, treeview, path, view_column, *args ) :
		self.__display_tech_at_path(path)	

	def __clear_widgets( self ) :
		self.refreshing = True

		name_id_txt = self.view.get_widget('tech_name_identifier_entry')
		name_id_txt.set_text( '' )

		name_str_txt = self.view.get_widget('tech_name_string_entry' )
		name_str_txt.set_text( '' )

		type_combo = self.view.get_widget('tech_type_combo_box')
		type_combo.child.set_text( 'TT_THEORY' )
		
		type_str_txt = self.view.get_widget('tech_type_string_entry')
		try:
			type_str_txt.set_text( strings[self.current_tech.type] )
		except KeyError :
			pass

		category_combo = self.view.get_widget('tech_category_combo_box' )
		category_combo.child.set_text( '' )
	
		cat_str_txt = self.view.get_widget('tech_cat_string_entry' )
		cat_str_txt.set_text( '' )

		self.desc_text_buffer.set_text("")

		cost_button = self.view.get_widget('tech_cost_button')
		cost_button.set_value( 0 )
		
		turns_button = self.view.get_widget('tech_turns_button')
		turns_button.set_value( 1 )

		icon_path = 'no_image.png'
		icont = gtk.gdk.pixbuf_new_from_file(icon_path)	
		tech_image = self.view.get_widget('tech_image')
		tech_image.set_from_pixbuf( icon )

		self.prereq_list.clear()
		self.effects_list.clear()
		self.items_list.clear()
			
		self.refreshing = False		

	def __display_current_tech( self ) :
		print "Displaying", self.current_tech.name
		self.refreshing = True
		self.__display_scalars()
		self.__display_lists()
		self.refreshing = False

	def __display_scalars( self ) :
		strings = application.instance.supported_languages['English']
		name_id_txt = self.view.get_widget('tech_name_identifier_entry')
		name_id_txt.set_text( self.current_tech.name )
		name_str_txt = self.view.get_widget('tech_name_string_entry' )
		try:
			name_str_txt.set_text( strings[self.current_tech.name] )
		except KeyError :
			name_str_txt.set_text("")

		type_combo = self.view.get_widget('tech_type_combo_box')
		type_combo.child.set_text( self.current_tech.type )
		
		type_str_txt = self.view.get_widget('tech_type_string_entry')
		try:
			type_str_txt.set_text( strings[self.current_tech.type] )
		except KeyError :
			pass

		category_combo = self.view.get_widget('tech_category_combo_box' )
		category_combo.child.set_text( self.current_tech.category )
	
		cat_str_txt = self.view.get_widget('tech_cat_string_entry' )
		try:
			cat_str_txt.set_text( strings[self.current_tech.category] )
		except KeyError :
			pass

		desc_str_txt = self.view.get_widget('description_view')
	
		try:
			self.desc_text_buffer.set_text( strings[self.current_tech.description] )
		except KeyError :
			self.desc_text_buffer.set_text("")		

		desc_str_txt.set_buffer( self.desc_text_buffer )

		cost_button = self.view.get_widget('tech_cost_button')
		cost_button.set_value( self.current_tech.research_cost )
		
		turns_button = self.view.get_widget('tech_turns_button')
		turns_button.set_value( self.current_tech.research_turns )	

		icon = self.__load_tech_icon()	
		
		tech_image = self.view.get_widget('tech_image')
		tech_image.set_from_pixbuf( icon )

	def __load_tech_icon( self ) :
		basedir = application.instance.dataset_folder
		artdir = 'data/art'
		if self.current_tech.graphic is None or len(self.current_tech.graphic) == 0:
			icon_path = 'no_image.png'
		else :
			icon_path = "%s/%s/%s"%(basedir,artdir,self.current_tech.graphic)
			if not os.path.exists(icon_path) :
				recoverable_error(self.window,"Specified icon file does not exist: %s"%icon_path)
				icon_path = 'no_image.png'
		return gtk.gdk.pixbuf_new_from_file(icon_path)	
	
	def __display_prerequisites( self ) :
		english_strings = application.instance.supported_languages['English']

		prereq_view = self.view.get_widget('prerequisites_view')
		prereq_view.set_model(None)
		self.prereq_list.clear()
		for prereq in self.current_tech.prerequisites :
			try:
				self.prereq_list.append((prereq,english_strings[prereq]))
			except KeyError :
				self.prereq_list.append((prereq,""))
					
		prereq_view.set_model(self.prereq_list)

	def __display_effects( self ) :
		english_strings = application.instance.supported_languages['English']
		effects_view = self.view.get_widget('effects_view')	
		effects_view.set_model(None)
		self.effects_list.clear()

		for effect in self.current_tech.effects :
			try:
				self.effects_list.append((effect,english_strings[effect]) )
			except KeyError :
				self.effects_list.append((effect,""))

		effects_view.set_model(self.effects_list)

	def __display_unlocked_items( self ) :
		english_strings = application.instance.supported_languages['English']
		items_view = self.view.get_widget('unlocked_items_view')
		items_view.set_model(None)
		self.items_list.clear()
		
		for item in self.current_tech.unlocked_items :
			try:
				self.items_list.append((item,english_strings[item]) )
			except KeyError :
				self.items_list.append((item,""))

		items_view.set_model(self.items_list)

	def __display_lists( self ) :
		self.__display_prerequisites()
		self.__display_unlocked_items()
		self.__display_effects()


	def __setup_prerequisites_view( self ) :
		prereqs_list = self.view.get_widget('prerequisites_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		prereqs_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		prereqs_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		prereqs_list.set_search_column(0)
	
	def __setup_unlocked_items_view( self ) :
		items_list = self.view.get_widget('unlocked_items_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		items_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		items_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		items_list.set_search_column(0)
	
	def __setup_effects_view( self ) :
		effects_list = self.view.get_widget('effects_view')

		identifier_column = gtk.TreeViewColumn('Identifier')
		effects_list.append_column( identifier_column )
		cell = gtk.CellRendererText()
		identifier_column.pack_start( cell, True )
		identifier_column.add_attribute(cell, 'text', 0)		
		identifier_column.set_sort_column_id(0)

		name_column = gtk.TreeViewColumn('Name')
		effects_list.append_column( name_column )	
		cell = gtk.CellRendererText()
		name_column.pack_start( cell, True )
		name_column.add_attribute( cell, 'text', 1)

		effects_list.set_search_column(0)


	def on_changes_approved( self, *args ) :
		self.__undo_bookkeeping()
		
		if len( self.techs_marked_for_death ) > 0 :
			tech_set = application.instance.tech_set
			for tech_id in self.techs_marked_for_death :
				print "Removing tech", tech_id
				tech_set.remove_tech( tech_id )
			application.instance.techs_modified = True
			application.instance.techs_modified = True


		if len(self.modified_techs) > 0 :
			print len(self.modified_techs), "techs modified"
			tech_set = application.instance.tech_set
			for tech in self.modified_techs.values() :
				tech_set.tech_entries[tech.name] = tech
			application.instance.techs_modified = True
			application.instance.strings_modified = True
		else :
			print "There were no changes..."

		self.window.hide()
		self.window.destroy()
	
	def on_changes_discarded( self, *args ) :
		self.window.hide()
		self.window.destroy()
	
	def on_find_tech( self, *args ) :
		not_implemented_feature_dialog(self.window)	
	
	def on_change_language_display( self, *args ) :
		not_implemented_feature_dialog(self.window)

	def on_window_deleted( self, widget, event, data=None ) :
		self.window.destroy()

	def show_all( self ) :
		self.__feed_data_on_widgets()
		self.window.show_all()

	def __feed_data_on_widgets( self ) :
		self.__read_tech_list()

	def __read_tech_list( self ) :

		self.view.get_widget('techs_list_view').set_model(None)
		self.tech_tree.clear()
		tech_set = application.instance.tech_set
		english_strings = application.instance.supported_languages['English']
		
		for category_identifier, tech_list in tech_set.tech_categories.items() :
			try:
				piter = self.tech_tree.append( None, (category_identifier, english_strings[category_identifier]) )
			except KeyError :
				piter = self.tech_tree.append( None, (category_identifier, "") )
			for tech in  tech_list :
				if tech in self.techs_marked_for_death : continue
				try:	
					self.tech_tree.append( piter, (tech, english_strings[tech]) )
				except KeyError :
					self.tech_tree.append( piter, (tech, "") )
		
		self.view.get_widget('techs_list_view').set_model(self.tech_tree)
