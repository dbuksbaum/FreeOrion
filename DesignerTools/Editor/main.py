import sys
from widgets.ui import *
import application

def main() :

	application.instance.ui_facade = InterfaceFacade()


	application.instance.ui_facade.start()
	


if __name__ == '__main__' :
	main()
else :
	print >> sys.stderr, "This is an executable script!"
	sys.exit(-1)

