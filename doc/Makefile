.PHONY: all
all: 

.PHONY: api_public
api_public:
	mkdir -p api/public && cd .. && doxygen doc/Doxyfile.public

.PHONY: api_private
api_private:
	mkdir -p api/private && cd .. && doxygen doc/Doxyfile.private
