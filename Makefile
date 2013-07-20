CC      = cc
OBJS    = bto_ir_cmd.o

bto_ir_cmd: $(OBJS)
	$(CC) -Wall -lusb-1.0 -o $@ $(OBJS)

.c.o:
	$(CC) -c $<

clean:
	@rm -f $(OBJS)
	@rm -f bto_ir_cmd
