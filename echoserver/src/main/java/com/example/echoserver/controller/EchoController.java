package com.example.echoserver.controller;

import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping
public class EchoController {
    
    @PostMapping("/echo")
    public String postmessage(@RequestBody String message) {
        
        return message;
    }

    @GetMapping("/auth")
    public String auth(@RequestParam String user, @RequestParam String password) {
        return "Hello: " + user + ":" + password;
    }

}
