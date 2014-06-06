NAME   := pnganography
HASH   := md5sum
BINDIR := /usr/local/bin

all: $(NAME)

$(NAME): $(NAME).c
	gcc -w -o $@ $< -lpng -lm

install: $(NAME)
	install -m 0755 $< $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(NAME)

test: $(NAME)
	./$< encode key.png secret-in.txt output.png
	./$< decode key.png output.png secret-out.txt
	$(HASH) secret-*.txt

clean:
	rm $(NAME) output.png secret-out.txt
