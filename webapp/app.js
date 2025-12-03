/**
 * MoQ WebRTC Phone Client
 * Implements WebSocket signaling and WebRTC media with MoQ
 */

class MoQPhone {
    constructor() {
        this.ws = null;
        this.userId = null;
        this.peerConnection = null;
        this.localStream = null;
        this.currentSessionId = null;
        this.currentPeer = null;
        this.callStartTime = null;
        this.callDurationInterval = null;
        
        // WebSocket server URL (adjust as needed)
        this.wsUrl = 'ws://localhost:8088';
        
        // WebRTC configuration
        this.rtcConfig = {
            iceServers: [
                { urls: 'stun:stun.l.google.com:19302' },
                { urls: 'stun:stun1.l.google.com:19302' }
            ]
        };
        
        this.initUI();
    }
    
    initUI() {
        // Get UI elements
        this.statusEl = document.getElementById('status');
        this.userIdInput = document.getElementById('userId');
        this.registerBtn = document.getElementById('registerBtn');
        this.callSection = document.getElementById('callSection');
        this.destIdInput = document.getElementById('destId');
        this.callBtn = document.getElementById('callBtn');
        this.hangupBtn = document.getElementById('hangupBtn');
        this.incomingCallEl = document.getElementById('incomingCall');
        this.overlayEl = document.getElementById('overlay');
        this.answerBtn = document.getElementById('answerBtn');
        this.rejectBtn = document.getElementById('rejectBtn');
        this.callerNameEl = document.getElementById('callerName');
        this.localAudioEl = document.getElementById('localAudio');
        this.remoteAudioEl = document.getElementById('remoteAudio');
        this.callInfoEl = document.getElementById('callInfo');
        this.callDurationEl = document.getElementById('callDuration');
        this.peerNameEl = document.getElementById('peerName');
        
        // Bind events
        this.registerBtn.addEventListener('click', () => this.register());
        this.callBtn.addEventListener('click', () => this.makeCall());
        this.hangupBtn.addEventListener('click', () => this.hangup());
        this.answerBtn.addEventListener('click', () => this.answerCall());
        this.rejectBtn.addEventListener('click', () => this.rejectCall());
        
        // Allow Enter key to trigger actions
        this.userIdInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.register();
        });
        this.destIdInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.makeCall();
        });
    }
    
    updateStatus(status, className) {
        this.statusEl.textContent = status;
        this.statusEl.className = 'status ' + className;
    }
    
    generateSessionId() {
        return 'session-' + Date.now() + '-' + Math.random().toString(36).substr(2, 9);
    }
    
    async register() {
        const userId = this.userIdInput.value.trim();
        if (!userId) {
            alert('Please enter a user ID');
            return;
        }
        
        this.userId = userId;
        this.updateStatus('Connecting...', 'calling');
        
        try {
            this.ws = new WebSocket(this.wsUrl);
            
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.ws.send(JSON.stringify({
                    type: 'register',
                    user_id: this.userId
                }));
            };
            
            this.ws.onmessage = (event) => this.handleSignaling(event);
            
            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.updateStatus('Disconnected', 'disconnected');
                this.callSection.style.display = 'none';
                this.registerBtn.disabled = false;
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.updateStatus('Connection failed', 'disconnected');
            };
            
        } catch (error) {
            console.error('Failed to connect:', error);
            alert('Failed to connect to signaling server');
            this.updateStatus('Disconnected', 'disconnected');
        }
    }
    
    handleSignaling(event) {
        const data = JSON.parse(event.data);
        console.log('Signaling message:', data);
        
        switch (data.type) {
            case 'registered':
                this.updateStatus(`Connected as ${this.userId}`, 'connected');
                this.registerBtn.disabled = true;
                this.userIdInput.disabled = true;
                this.callSection.style.display = 'block';
                break;
                
            case 'incoming_call':
                this.handleIncomingCall(data);
                break;
                
            case 'ringing':
                this.updateStatus('Ringing...', 'calling');
                break;
                
            case 'call_answered':
                this.handleCallAnswered(data);
                break;
                
            case 'call_ended':
                this.handleCallEnded(data);
                break;
                
            case 'call_failed':
                alert('Call failed: ' + data.reason);
                this.updateStatus(`Connected as ${this.userId}`, 'connected');
                this.resetCallUI();
                break;
                
            case 'sdp_offer':
                this.handleOffer(data);
                break;
                
            case 'sdp_answer':
                this.handleAnswer(data);
                break;
                
            case 'ice_candidate':
                this.handleIceCandidate(data);
                break;
        }
    }
    
    handleIncomingCall(data) {
        this.currentSessionId = data.session_id;
        this.currentPeer = data.from;
        
        this.callerNameEl.textContent = `${data.from} is calling...`;
        this.incomingCallEl.classList.add('show');
        this.overlayEl.classList.add('show');
        
        // Play ringtone (optional)
        this.playRingtone();
    }
    
    async answerCall() {
        this.incomingCallEl.classList.remove('show');
        this.overlayEl.classList.remove('show');
        this.stopRingtone();
        
        this.updateStatus('Answering...', 'calling');
        
        // Send answer message
        this.ws.send(JSON.stringify({
            type: 'answer',
            session_id: this.currentSessionId,
            from: this.userId
        }));
        
        // Setup WebRTC
        await this.setupWebRTC(false);
        
        this.updateStatus('In call', 'incall');
        this.showCallUI();
    }
    
    rejectCall() {
        this.incomingCallEl.classList.remove('show');
        this.overlayEl.classList.remove('show');
        this.stopRingtone();
        
        this.ws.send(JSON.stringify({
            type: 'hangup',
            session_id: this.currentSessionId
        }));
        
        this.currentSessionId = null;
        this.currentPeer = null;
    }
    
    async makeCall() {
        const dest = this.destIdInput.value.trim();
        if (!dest) {
            alert('Please enter a user ID to call');
            return;
        }
        
        if (dest === this.userId) {
            alert('Cannot call yourself');
            return;
        }
        
        this.currentPeer = dest;
        this.currentSessionId = this.generateSessionId();
        
        this.updateStatus('Calling...', 'calling');
        this.callBtn.style.display = 'none';
        this.hangupBtn.style.display = 'block';
        
        // Setup WebRTC
        await this.setupWebRTC(true);
        
        // Send call message
        this.ws.send(JSON.stringify({
            type: 'call',
            session_id: this.currentSessionId,
            dest: dest,
            from: this.userId
        }));
    }
    
    async setupWebRTC(isInitiator) {
        try {
            // Get local media (audio only for phone)
            this.localStream = await navigator.mediaDevices.getUserMedia({
                audio: {
                    echoCancellation: true,
                    noiseSuppression: true,
                    autoGainControl: true
                },
                video: false
            });
            
            this.localAudioEl.srcObject = this.localStream;
            
            // Create peer connection
            this.peerConnection = new RTCPeerConnection(this.rtcConfig);
            
            // Add local stream
            this.localStream.getTracks().forEach(track => {
                this.peerConnection.addTrack(track, this.localStream);
            });
            
            // Handle incoming tracks
            this.peerConnection.ontrack = (event) => {
                console.log('Received remote track');
                this.remoteAudioEl.srcObject = event.streams[0];
            };
            
            // Handle ICE candidates
            this.peerConnection.onicecandidate = (event) => {
                if (event.candidate) {
                    this.ws.send(JSON.stringify({
                        type: 'ice_candidate',
                        candidate: event.candidate,
                        session_id: this.currentSessionId,
                        dest: this.currentPeer
                    }));
                }
            };
            
            // Handle connection state changes
            this.peerConnection.onconnectionstatechange = () => {
                console.log('Connection state:', this.peerConnection.connectionState);
                if (this.peerConnection.connectionState === 'connected') {
                    this.updateStatus('In call', 'incall');
                    this.showCallUI();
                }
            };
            
            // If initiator, create offer
            if (isInitiator) {
                const offer = await this.peerConnection.createOffer();
                await this.peerConnection.setLocalDescription(offer);
                
                this.ws.send(JSON.stringify({
                    type: 'sdp_offer',
                    sdp: offer,
                    session_id: this.currentSessionId,
                    dest: this.currentPeer,
                    from: this.userId
                }));
            }
            
        } catch (error) {
            console.error('Error setting up WebRTC:', error);
            alert('Failed to access microphone or setup call');
            this.hangup();
        }
    }
    
    async handleOffer(data) {
        if (!this.peerConnection) {
            await this.setupWebRTC(false);
        }
        
        await this.peerConnection.setRemoteDescription(new RTCSessionDescription(data.sdp));
        
        const answer = await this.peerConnection.createAnswer();
        await this.peerConnection.setLocalDescription(answer);
        
        this.ws.send(JSON.stringify({
            type: 'sdp_answer',
            sdp: answer,
            session_id: this.currentSessionId,
            dest: data.from
        }));
    }
    
    async handleAnswer(data) {
        if (this.peerConnection) {
            await this.peerConnection.setRemoteDescription(new RTCSessionDescription(data.sdp));
        }
    }
    
    async handleIceCandidate(data) {
        if (this.peerConnection && data.candidate) {
            try {
                await this.peerConnection.addIceCandidate(new RTCIceCandidate(data.candidate));
            } catch (error) {
                console.error('Error adding ICE candidate:', error);
            }
        }
    }
    
    handleCallAnswered(data) {
        this.updateStatus('Call answered', 'incall');
    }
    
    handleCallEnded(data) {
        this.hangup();
    }
    
    hangup() {
        console.log('Hanging up');
        
        // Send hangup message
        if (this.ws && this.currentSessionId) {
            this.ws.send(JSON.stringify({
                type: 'hangup',
                session_id: this.currentSessionId
            }));
        }
        
        // Close peer connection
        if (this.peerConnection) {
            this.peerConnection.close();
            this.peerConnection = null;
        }
        
        // Stop local stream
        if (this.localStream) {
            this.localStream.getTracks().forEach(track => track.stop());
            this.localStream = null;
        }
        
        // Reset UI
        this.updateStatus(`Connected as ${this.userId}`, 'connected');
        this.resetCallUI();
        
        this.currentSessionId = null;
        this.currentPeer = null;
        
        this.stopCallDuration();
    }
    
    showCallUI() {
        this.callBtn.style.display = 'none';
        this.hangupBtn.style.display = 'block';
        this.destIdInput.disabled = true;
        this.callInfoEl.classList.add('show');
        this.peerNameEl.textContent = this.currentPeer;
        
        this.startCallDuration();
    }
    
    resetCallUI() {
        this.callBtn.style.display = 'block';
        this.hangupBtn.style.display = 'none';
        this.destIdInput.disabled = false;
        this.callInfoEl.classList.remove('show');
    }
    
    startCallDuration() {
        this.callStartTime = Date.now();
        this.callDurationInterval = setInterval(() => {
            const duration = Math.floor((Date.now() - this.callStartTime) / 1000);
            const minutes = Math.floor(duration / 60).toString().padStart(2, '0');
            const seconds = (duration % 60).toString().padStart(2, '0');
            this.callDurationEl.textContent = `${minutes}:${seconds}`;
        }, 1000);
    }
    
    stopCallDuration() {
        if (this.callDurationInterval) {
            clearInterval(this.callDurationInterval);
            this.callDurationInterval = null;
        }
        this.callDurationEl.textContent = '00:00';
    }
    
    playRingtone() {
        // Optionally implement ringtone
        // For simplicity, we'll skip this for now
    }
    
    stopRingtone() {
        // Stop ringtone if implemented
    }
}

// Initialize the phone app
const phone = new MoQPhone();
console.log('MoQ Phone initialized');
