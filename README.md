# Computer_architecture

![스크린샷 2024-06-20 131456](https://github.com/ADALIV/Computer_architecture/assets/154600451/4054c075-793d-4087-9492-772534c1afdb)<br>Summation result<br><br>
![스크린샷 2024-06-20 131358](https://github.com/ADALIV/Computer_architecture/assets/154600451/8f27b278-872e-4646-84ef-f62c2f7b2005)<br>Summation with recursive result<br><br>
![스크린샷 2024-06-20 131134](https://github.com/ADALIV/Computer_architecture/assets/154600451/3d03deb7-1c91-4484-866a-bc7be7a8a6d5)<br>GCD result<br><br>
![스크린샷 2024-06-20 130942](https://github.com/ADALIV/Computer_architecture/assets/154600451/75a8b070-9741-43e9-80b8-34275ecf3819)<br>input4 result<br><br>

<br><br>

<p>
  In Linux environment, to get input binary file for sum_re.c, use this commands.

  <br>Make object file from c source file.
  ```
  mips-linux-gnu-gcc -c -o sum_re.o sum_re.c
  ```
  <br>Make input binary file from object file.
  ```
  mips-linux-gnu-objcopy -O binary -j .text sum_re.o input.bin
  ```
  <br>To see MIPS ISA commands, use this.
  ```
  mips-linux-gnu-objdump -d sum_re.o
  ```
</p>

<br><br>

When there is a jump instruction that goes back and forth between functions, please modify binary file yourself.<br>
0c 00 00 00  &rarr;  always jump to main, which occur infinite loop<br>
0c 00 00 0f  &rarr;  jump target function is 0000003c, (0000003c >> 2) = 0000000f<br>

<br><br>

<p>
  <ul>
    <li>Pipeline architecture</li>
    <ul>
      <li>Data hazard  &rarr;  forwarding</li>
      <li>Control hazard  &rarr;  stalling</li>
    </ul>
    <li>Cache architecture</li>
    <ul>
      <li>Fully-associative cache</li>
      <li>SCA(second chance algorithm)</li>
    </ul>
  </ul>
</p>
